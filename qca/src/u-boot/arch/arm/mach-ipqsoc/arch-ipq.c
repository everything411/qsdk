// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <asm/cache.h>
#include <asm/system.h>
#include <common.h>
#include <cpu_func.h>
#include <image.h>
#include <asm/global_data.h>
#include <asm/sections.h>

DECLARE_GLOBAL_DATA_PTR;

#define UBOOT_CACHE_SETUP		0x100e
#define GEN_CACHE_SETUP			0x101e

int arch_setup_dest_addr(void)
{
	gd->relocaddr = CONFIG_TEXT_BASE;
	gd->reloc_off = gd->relocaddr - CONFIG_TEXT_BASE;

	return 0;
}

int arm_reserve_mmu(void)
{
	/* reserve TLB table */
	gd->arch.tlb_size = PGTABLE_SIZE;
	gd->arch.tlb_addr = CONFIG_TEXT_BASE + gd->mon_len;
	gd->arch.tlb_addr += (0x10000 - 1);
	gd->arch.tlb_addr &= ~(0x10000 - 1);

	return 0;
}

/*
 * Flush range from all levels of d-cache/unified-cache.
 * Affects the range,
 *	if cache is algined,
 *		from : start
 *		to   : start + size - 1
 *	if cache is not aligned,
 *		from : start - cache aligne address
 *		to   : start + size - 1 + cache aligne address
 */
void flush_cache(unsigned long start, unsigned long size)
{
	unsigned long stop = start + size;

	if (start & (CONFIG_SYS_CACHELINE_SIZE - 1))
		start = start & ~(CONFIG_SYS_CACHELINE_SIZE - 1);

	if (stop & (CONFIG_SYS_CACHELINE_SIZE - 1))
		stop = CONFIG_SYS_CACHELINE_SIZE +
			(stop & ~(CONFIG_SYS_CACHELINE_SIZE - 1));

	flush_dcache_range(start, stop);
}

#ifdef CONFIG_OF_SEPARATE
#ifdef CONFIG_MULTI_DTB_FIT_NO_COMPRESSION
int calc_fdt_blob_size(void)
{
	int size = 0;
	void * fdt_blob = (ulong *)&_end;
	int noffset = 0, image_noffset = 0;

	size = fdt_totalsize(fdt_blob);
	image_noffset = fdt_path_offset(fdt_blob, FIT_IMAGES_PATH);
	if (image_noffset < 0) {
		printf("Can't find images parent node '%s' (%s)\n",
		       FIT_IMAGES_PATH, fdt_strerror(image_noffset));
		return size;
	}

	fdt_for_each_subnode(noffset, fdt_blob, image_noffset) {
		int data_size = 0;
		fit_image_get_data_size(fdt_blob, noffset, &data_size);
		size += data_size;
	}

	return size;
}
#endif /* CONFIG_MULTI_DTB_FIT_NO_COMPRESSION */
#endif /* CONFIG_OF_SEPARATE */

int mach_cpu_init(void)
{
	gd->flags |= GD_FLG_SKIP_RELOC;
#ifdef CONFIG_OF_SEPARATE
#ifdef CONFIG_MULTI_DTB_FIT_NO_COMPRESSION
	gd->mon_len += calc_fdt_blob_size();
#endif /* CONFIG_MULTI_DTB_FIT_NO_COMPRESSION */
#endif
	return 0;
}

#ifndef CONFIG_ARM64
int arch_cpu_init(void)
{
	u32 val;
	/* Read SCTLR */
	asm volatile ("mrc p15, 0, %0, c1, c0, 0" : "=r" (val));
	/* set the cp15 barrier enable bit */
	val |= 0x20;
	/* write back to SCTLR */
	asm volatile ("mcr p15, 0, %0, c1, c0, 0" : : "r" (val));

	return 0;
}

void dram_bank_mmu_setup(int bank)
{
	struct bd_info *bd = gd->bd;
	int i;
	uint32_t ram_size;

	/*
	 * check and limit the DDR region to avoid overflow
	 */
	if (((uint64_t)bd->bi_dram[bank].start +
		bd->bi_dram[bank].size) > ULONG_MAX) {
		ram_size = bd->bi_dram[bank].start +
				(ULONG_MAX - bd->bi_dram[bank].start);
	} else {
		ram_size = bd->bi_dram[bank].start + bd->bi_dram[bank].size;
	}

	/* bd->bi_dram is available only after relocation */
	if ((gd->flags & GD_FLG_RELOC) == 0)
		return;

	debug("%s: bank: %d\n", __func__, bank);
	for (i = bd->bi_dram[bank].start >> 20;
		i < ram_size >> 20;
		i++) {
		/* Set XN bit for all dram regions except uboot code region */
		if (i >= (CONFIG_TEXT_BASE >> 20) && i <
				((CONFIG_TEXT_BASE + CONFIG_TEXT_SIZE) >> 20))
			set_section_dcache(i, UBOOT_CACHE_SETUP);
		else
			set_section_dcache(i, GEN_CACHE_SETUP);
	}
}
#endif
