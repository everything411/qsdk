// SPDX-License-Identifier: GPL-2.0+
/*
 * Common initialisation for Qualcomm ipq boards.
 *
 * Copyright (c) 2024 Linaro Ltd.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * Author: Caleb Connolly <caleb.connolly@linaro.org>
 */

#ifdef CONFIG_ARM64
#include <asm/armv8/mmu.h>
#endif
#include <mach/ipq.h>
#include <asm/system.h>
#include <asm/cache.h>
#include <mach/smem_info.h>
#include <linux/err.h>
#include <linux/psci.h>
#include <linux/sizes.h>
#include <sysreset.h>
#include <cpu_func.h>
#include <dm/device-internal.h>
#include <dm/device.h>
#include <dm/read.h>
#include <env.h>
#include <fdt_support.h>

/******************************************************************
 * Globals and constant
 *****************************************************************/
DECLARE_GLOBAL_DATA_PTR;

#ifndef BOOT_PARAMS_ADDR
#define BOOT_PARAMS_ADDR			(KERNEL_START_ADDR + 0x100)
#endif
uint32_t g_board_machid __section(".data");
struct ipq_board_info *ipq_bdinfo;
uint32_t g_recovery_path __section(".data");
#if defined(CONFIG_ENV_IS_IN_SPI_FLASH)
uint32_t g_env_offset __section(".data") = 0;
#endif

#define DTS_SUFFIX			"-ub"
#define SZ_96K				0x18000

/****************************************************************
 * Weak function definition
 * this placeholder for generic weak function definition.
 ****************************************************************/
__weak void ipq_board_early_init_f(void) {}

__weak int ipq_uboot_fdt_fixup(void *blob, enum fixup_type)
{
	return 0;
}

__weak void board_cache_init(void)
{
	icache_enable();
#if !CONFIG_IS_ENABLED(SYS_DCACHE_OFF)
	dcache_enable();
#endif
}

/*
 * Global func definition
 */
struct ipq_board_info *ipq_get_bdinfo(void)
{
	return ipq_bdinfo;
}

#if defined(CONFIG_SMEM) && defined(CONFIG_MSM_SMEM)
struct ipq_smem_flash_info *ipq_get_smem_info(void)
{
	if (ipq_bdinfo)
		return &ipq_bdinfo->smem_info;
	else
		return NULL;
}

struct soc_info *ipq_get_socinfo(void)
{
	if (ipq_bdinfo)
		return &ipq_bdinfo->ipq_socinfo;
	else
		return NULL;
}

struct smem_ptable *ipq_get_part_table(void)
{
	if (ipq_bdinfo)
		return ipq_bdinfo->ptable;
	else
		return NULL;
}
#else
struct ipq_smem_flash_info *ipq_get_smem_info(void)
{
	return NULL;
}

struct soc_info *ipq_get_socinfo(void)
{
	return NULL;
}

struct smem_ptable *ipq_get_part_table(void)
{
	return NULL;
}
#endif

#if defined(CONFIG_SPL)
/**
 * ipq_spl_mem_map - Memory map for SPL
 *
 * This memory map is used by the SPL to configure the MMU. It defines
 * regions of memory that are mapped into the virtual address space.
 * The memory map includes device registers, GIC registers, TME-L IPC
 * registers, BOOTIMEM, OCIMEM, and DDR regions.
 *
 * Each region is defined with its virtual address, physical address,
 * size, and memory attributes.
 */
static struct mm_region ipq_spl_mem_map[] = {
	{
		/*
		 * Device registers
		 */
		.virt = IPQ_SPL_DEVICE_REG_BASE,
		.phys = IPQ_SPL_DEVICE_REG_BASE,
		.size = IPQ_SPL_DEVICE_REG_SIZE,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/*
		 * Device registers 2
		 */
		.virt = IPQ_SPL_DEVICE_REG2_BASE,
		.phys = IPQ_SPL_DEVICE_REG2_BASE,
		.size = IPQ_SPL_DEVICE_REG2_SIZE,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/*
		 * OCIMEM Region
		 */
		.virt = IPQ_SPL_OCIMEM_REG_BASE,
		.phys = IPQ_SPL_OCIMEM_REG_BASE,
		.size = IPQ_SPL_OCIMEM_REG_SIZE,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/*
		 * USB registers
		 */
		.virt = IPQ_SPL_USB_PRIM_REG_BASE,
		.phys = IPQ_SPL_USB_PRIM_REG_BASE,
		.size = IPQ_SPL_USB_PRIM_REG_SIZE,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/*
		 * BOOTIMEM Region
		 */
		.virt = IPQ_SPL_BOOTIMEM_REG_BASE,
		.phys = IPQ_SPL_BOOTIMEM_REG_BASE,
		.size = IPQ_SPL_BOOTIMEM_REG_SIZE,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/*
		 * TME-L IPC registers
		 */
		.virt = IPQ_SPL_TMEL_MBOX_REG_BASE,
		.phys = IPQ_SPL_TMEL_MBOX_REG_BASE,
		.size = IPQ_SPL_TMEL_MBOX_REG_SIZE,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/*
		 * DDR Region
		 */
		.virt = IPQ_SPL_DDR_MEM_BASE,
		.phys = IPQ_SPL_DDR_MEM_BASE,
		.size = IPQ_SPL_DDR_MEM_SIZE,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	},
#ifdef CONFIG_SPL_DDR2_MEM_BASE
       {
               /*
                * DDR 2 Region
                */
               .virt = IPQ_SPL_DDR2_MEM_BASE,
               .phys = IPQ_SPL_DDR2_MEM_BASE,
               .size = IPQ_SPL_DDR2_MEM_SIZE,
               .attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
                        PTE_BLOCK_INNER_SHARE |
                        PTE_BLOCK_PXN | PTE_BLOCK_UXN
       },
#endif
	{
		/*
		 * SPL Text Region
		 */
		.virt = CONFIG_SPL_TEXT_BASE,
		.phys = CONFIG_SPL_TEXT_BASE,
		.size = CONFIG_SPL_TEXT_SIZE,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		/*
		 * QCLIB Text Region
		 */
		.virt = IPQ_SPL_QCLIB_TEXT_BASE,
		.phys = IPQ_SPL_QCLIB_TEXT_BASE,
		.size = IPQ_SPL_QCLIB_TEXT_SIZE,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		/*
		 * QCLIB DSF Text Region
		 */
		.virt = IPQ_SPL_QCLIB_DSF_TEXT_BASE,
		.phys = IPQ_SPL_QCLIB_DSF_TEXT_BASE,
		.size = IPQ_SPL_QCLIB_DSF_TEXT_SIZE,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		/*
		 * TFA Text Region
		 */
		.virt = IPQ_SPL_TFA_TEXT_BASE,
		.phys = IPQ_SPL_TFA_TEXT_BASE,
		.size = IPQ_SPL_TFA_TEXT_SIZE,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		/*
		 * List terminator
		 */
		0,
	}
};

/*
 * Assign the mem_map instance to ipq_spl_mem_map,
 * which defines the SPL memory map
 */
struct mm_region *mem_map = ipq_spl_mem_map;

/**
 * get_page_table_size() - Get the size of the page table.
 *
 * This function returns the size of the page table used by the MMU.
 * It is used by the U-Boot framework to allocate appropriate memory for
 * page table structures.
 *
 * Returns:
 *	Size of the page table in bytes.
 */
u64 get_page_table_size(void)
{
	return SZ_64K;
}

#else
static void check_recovery_mode(void)
{
	if (g_recovery_path == 1)
		gd->board_type |= RECOVERY_MODE;
#if defined(CRASH_DUMP_ADDR_IMEM)
	else {
		g_recovery_path = readl(CRASH_DUMP_ADDR_IMEM) & 0xffffffff;
		gd->board_type |= (g_recovery_path == MAGIC_RECOVERY_PATH) ?
		RECOVERY_MODE : 0;
	}
#endif
}

/*
 * List of reserved memory node prefixes that should be remapped as non-cacheable.
 * Add new node prefixes here to extend the list of secure regions.
 *
 * Note: These are node names (e.g., "tz@8a600000"), not labels.
 * Device tree labels (e.g., "tz:") are not returned by fdt_get_name().
 */
static const char *secure_region_prefixes[] = {
	"tz@",
	"tz_mem",
	"tz_region",
	"smem",
	"tfa",
	"optee",
	"atf",
};

/**
 * process_secure_region_node() - Check and parse a single reserved-memory node
 * @node: Device tree node offset
 * @base: Output - base address (if region matches)
 * @size: Output - size (if region matches)
 *
 * This helper function checks if a given device tree node represents a secure
 * region that should be mapped as non-cacheable. It validates the node has the
 * "no-map" property, matches one of the secure region prefixes, and parses the
 * base address and size from the reg property.
 *
 * Returns: 1 if this is a matching secure region, 0 otherwise
 */
static int process_secure_region_node(int node, u64 *base, u64 *size)
{
	const void *fdt = gd->fdt_blob;
	const fdt32_t *reg;
	const char *node_name;
	int len, addr_cells, size_cells, parent;
	u32 j;
	bool is_target_node = false;

	/* Only process nodes with "no-map" property */
	if (!fdt_getprop(fdt, node, "no-map", NULL))
		return 0;

	/* Get node name and check if it matches any target prefix */
	node_name = fdt_get_name(fdt, node, NULL);
	if (!node_name)
		return 0;

	/* Check if node name matches any of the secure region prefixes */
	for (j = 0; j < ARRAY_SIZE(secure_region_prefixes); j++) {
		if (strncmp(node_name, secure_region_prefixes[j],
			    strlen(secure_region_prefixes[j])) == 0) {
			is_target_node = true;
			break;
		}
	}

	/* Skip if not a target node */
	if (!is_target_node)
		return 0;

	/* Get reg property */
	reg = fdt_getprop(fdt, node, "reg", &len);
	if (!reg)
		return 0;

	/* Get #address-cells and #size-cells from parent node */
	parent = fdt_parent_offset(fdt, node);
	if (parent < 0)
		return 0;

	addr_cells = fdt_address_cells(fdt, parent);
	size_cells = fdt_size_cells(fdt, parent);

	if (addr_cells < 1 || addr_cells > 2 || size_cells < 1 || size_cells > 2)
		return 0;

	/* Verify we have enough cells */
	if (len < (addr_cells + size_cells) * sizeof(fdt32_t))
		return 0;

	/* Parse base address based on #address-cells */
	if (addr_cells == 2)
		*base = ((u64)fdt32_to_cpu(reg[0]) << 32) | fdt32_to_cpu(reg[1]);
	else
		*base = fdt32_to_cpu(reg[0]);

	/* Parse size based on #size-cells */
	if (size_cells == 2)
		*size = ((u64)fdt32_to_cpu(reg[addr_cells]) << 32) |
			fdt32_to_cpu(reg[addr_cells + 1]);
	else
		*size = fdt32_to_cpu(reg[addr_cells]);

	/* Skip zero-size regions */
	if (!*size)
		return 0;

	/* Validate address range is within addressable space */
	if (*base > ULONG_MAX || *size > ULONG_MAX - *base)
		return 0;

	return 1;
}

#ifdef CONFIG_ARM64
/* Maximum number of secure regions we expect to find in device tree */
#define MAX_SECURE_REGIONS 10

/*
 * 1 peripheral + CONFIG_NR_DRAM_BANKS DRAM + up to MAX_SECURE_REGIONS secure
 * regions + 1 relocated U-Boot text + 1 pre-reloc U-Boot text
 * (IPQ_DYNAMIC_RELOCATION only) + 1 terminator
 */
static struct mm_region ipq_mem_map[CONFIG_NR_DRAM_BANKS + 4 + MAX_SECURE_REGIONS]
	__section(".data") = { { 0 } };
struct mm_region *mem_map = ipq_mem_map;

/**
 * ipq_remap_secure_regions() - Remap secure memory regions as non-cacheable
 * @next_idx: Pointer to the next available mem_map index (ARM64 only)
 *
 * Scans the device tree /reserved-memory node for specific secure region nodes
 * and remaps those regions as DCACHE_OFF in the MMU. This prevents cache
 * writebacks to secure regions that would trigger XPU violations when accessed
 * from Non-Secure U-Boot.
 *
 * ARM64: Adds secure regions as separate Device-nGnRnE entries in mem_map array.
 */
static void ipq_remap_secure_regions(int *next_idx)
{
	const void *fdt = gd->fdt_blob;
	int node, mem_node;
	u64 base, size;
	int i = *next_idx;
	int secure_count = 0;

	if (!fdt)
		return;

	mem_node = fdt_path_offset(fdt, "/reserved-memory");
	if (mem_node < 0)
		return;

	/* Iterate through reserved-memory child nodes */
	fdt_for_each_subnode(node, fdt, mem_node) {
		if (process_secure_region_node(node, &base, &size)) {
			/* 3 trailing slots: relocated text, pre-reloc text, terminator */
			if (secure_count >= MAX_SECURE_REGIONS ||
			    i >= ARRAY_SIZE(ipq_mem_map) - 3) {
				printf("Warning: Not enough mem_map entries for all secure regions\n");
				break;
			}

			/* Add this secure region as Device-nGnRnE (non-cacheable) */
			mem_map[i].phys = base;
			mem_map[i].virt = base;
			mem_map[i].size = size;
			mem_map[i].attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
					   PTE_BLOCK_NON_SHARE |
					   PTE_BLOCK_PXN | PTE_BLOCK_UXN;

			i++;
			secure_count++;
		}
	}

	*next_idx = i;
}

static void build_mem_map(void)
{
	int i, j;

	/*
	 * Ensure the peripheral block is sized to correctly cover the address
	 * range up to the first memory bank.
	 * Don't map the first page to ensure that we actually trigger an abort
	 * on a null pointer access rather than just hanging.
	 */
	mem_map[0].phys = 0x1000;
	mem_map[0].virt = mem_map[0].phys;
	mem_map[0].size = gd->bd->bi_dram[0].start - mem_map[0].phys;
	mem_map[0].attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN;

	for (i = 1, j = 0; j < CONFIG_NR_DRAM_BANKS &&
		gd->bd->bi_dram[j].size; i++, j++) {
		mem_map[i].phys = gd->bd->bi_dram[j].start;
		mem_map[i].virt = mem_map[i].phys;
		mem_map[i].size = gd->bd->bi_dram[j].size;
		mem_map[i].attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
				PTE_BLOCK_INNER_SHARE |
				PTE_BLOCK_PXN | PTE_BLOCK_UXN;
	}

	/*
	 * Add secure regions as Device-nGnRnE (non-cacheable) entries
	 * This prevents cache writebacks to secure regions that would
	 * trigger XPU violations from Non-Secure U-Boot
	 */
	ipq_remap_secure_regions(&i);

	/*
	 * Mark the relocated U-Boot text region as executable.
	 */
	mem_map[i].phys = round_down(gd->relocaddr, SZ_4K);
	mem_map[i].virt = mem_map[i].phys;
	mem_map[i].size =
		round_up(gd->relocaddr + gd->mon_len, SZ_4K) - mem_map[i].phys;
	mem_map[i].attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
				PTE_BLOCK_INNER_SHARE;
	i++;

	/*
	 * Also mark the pre-relocation text region (CONFIG_TEXT_BASE) as
	 * executable. Pre-relocation data structures (e.g. serial driver state)
	 * remain at this address until the serial driver is re-probed after
	 * relocation. Skip this entry when running in-place (skip-reloc path)
	 * since gd->relocaddr already equals CONFIG_TEXT_BASE in that case.
	 */
#if CONFIG_IS_ENABLED(IPQ_DYNAMIC_RELOCATION)
	if (gd->relocaddr != CONFIG_TEXT_BASE) {
		mem_map[i].phys = round_down(CONFIG_TEXT_BASE, SZ_4K);
		mem_map[i].virt = mem_map[i].phys;
		mem_map[i].size =
			round_up(CONFIG_TEXT_BASE + gd->mon_len, SZ_4K) - mem_map[i].phys;
		mem_map[i].attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
					PTE_BLOCK_INNER_SHARE;
		i++;
	}
#endif

	mem_map[i].phys = UINT64_MAX;
	mem_map[i].size = 0;
	mem_map[i].attrs = 0;

#ifdef DEBUG
	debug("Configured memory map:\n");
	for (i = 0; mem_map[i].size; i++)
		debug("  0x%016llx - 0x%016llx: entry %d\n",
		      mem_map[i].phys, mem_map[i].phys + mem_map[i].size, i);
#endif
}

u64 get_page_table_size(void)
{
	return SZ_96K;
}

void enable_caches(void)
{
	u64 tlb_addr = gd->arch.tlb_addr;
	u64 tlb_size = gd->arch.tlb_size;
	u64 pt_size;

	gd->arch.tlb_fillptr = tlb_addr;

	build_mem_map();

	/* Create normal system page tables */
	setup_pgtables();

	pt_size = (uintptr_t)gd->arch.tlb_fillptr -
			(uintptr_t)gd->arch.tlb_addr;
	debug("Primary pagetable size: %lluKiB\n", pt_size / 1024);

	/* Create emergency page tables */
	gd->arch.tlb_size -= pt_size;
	gd->arch.tlb_addr = gd->arch.tlb_fillptr;
	setup_pgtables();
	gd->arch.tlb_emerg = gd->arch.tlb_addr;
	gd->arch.tlb_addr = tlb_addr;
	gd->arch.tlb_size = tlb_size;

	check_recovery_mode();
	board_cache_init();
}

#else /* CONFIG_ARMV7 */
#define EXEC_CACHE_OPTION				0x100e
#define NONEXEC_CACHE_OPTION				0x101e

/**
 * ipq_remap_secure_regions() - Remap secure memory regions as non-cacheable
 *
 * Scans the device tree /reserved-memory node for specific secure region nodes
 * and remaps those regions as DCACHE_OFF in the MMU. This prevents cache
 * writebacks to secure regions that would trigger XPU violations when accessed
 * from Non-Secure U-Boot.
 *
 * Only remaps nodes whose names match the prefixes in secure_region_prefixes[].
 * Used by ARM32 architecture.
 */
static void ipq_remap_secure_regions(void)
{
	const void *fdt = gd->fdt_blob;
	int node, mem_node;
	u64 base, size;

	if (!fdt)
		return;

	mem_node = fdt_path_offset(fdt, "/reserved-memory");
	if (mem_node < 0)
		return;

	/* Iterate through reserved-memory child nodes */
	fdt_for_each_subnode(node, fdt, mem_node) {
		if (process_secure_region_node(node, &base, &size)) {
			/* ARM32: Use set_section_dcache for each 1MB section */
			u32 start_section, end_section, i;

			/* Calculate section range for this region */
			start_section = base >> MMU_SECTION_SHIFT;
			end_section = (base + size - 1) >> MMU_SECTION_SHIFT;

			/* Remap all sections in this region as non-cacheable */
			for (i = start_section; i <= end_section; i++)
				set_section_dcache(i, DCACHE_OFF);
		}
	}
}

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

static bool isexecute_configured(int i)
{
	bool done = false;

	/*
	 * Set XN bit for all dram regions except uboot code region
	 */
	if (i >= (round_down(gd->relocaddr, SZ_1M) >> MMU_SECTION_SHIFT) &&
		i < (round_up(gd->relocaddr + gd->mon_len, SZ_1M)
			>> MMU_SECTION_SHIFT)) {
		set_section_dcache(i, EXEC_CACHE_OPTION);
		done = true;
	}

#if CONFIG_IS_ENABLED(IPQ_DYNAMIC_RELOCATION)
	/*
	 * Also mark the pre-relocation text region (CONFIG_TEXT_BASE) as
	 * executable. Pre-relocation data structures (e.g. serial driver state)
	 * remain at this address until the serial driver is re-probed after
	 * relocation. Skip when running in-place since relocaddr equals
	 * CONFIG_TEXT_BASE in that case and the check above already covers it.
	 */
	if (!done && gd->relocaddr != CONFIG_TEXT_BASE &&
	    i >= (round_down(CONFIG_TEXT_BASE, SZ_1M) >> MMU_SECTION_SHIFT) &&
	    i < (round_up(CONFIG_TEXT_BASE + gd->mon_len, SZ_1M)
			>> MMU_SECTION_SHIFT)) {
		set_section_dcache(i, EXEC_CACHE_OPTION);
		done = true;
	}
#endif

	return done;
}

void dram_bank_mmu_setup(int bank)
{
	struct bd_info *bd = gd->bd;
	int i;
	uint32_t dram_size;

	/* bd->bi_dram is available only after relocation */
	if ((gd->flags & GD_FLG_RELOC) == 0)
		return;

	/*
	 * check and limit the DDR region to avoid overflow
	 */
	if (((uint64_t)bd->bi_dram[bank].start +
		bd->bi_dram[bank].size) > ULONG_MAX) {
		dram_size = bd->bi_dram[bank].start +
				(ULONG_MAX - bd->bi_dram[bank].size);
	} else {
		dram_size = bd->bi_dram[bank].start + bd->bi_dram[bank].size;
	}

	dram_size >>= MMU_SECTION_SHIFT;

	debug("%s: bank: %d\n", __func__, bank);
	for (i = bd->bi_dram[bank].start >> MMU_SECTION_SHIFT; i < dram_size;
		i++) {
		if (isexecute_configured(i))
			continue;

		set_section_dcache(i, NONEXEC_CACHE_OPTION);
	}

	/*
	 * Phase 2: Remap secure regions as non-cacheable
	 * After marking all sections as cacheable, selectively remap
	 * secure regions (ATF, OP-TEE, SMEM) as DCACHE_OFF to prevent
	 * XPU violations from cache writebacks.
	 */
	ipq_remap_secure_regions();
}

void enable_caches(void)
{
	check_recovery_mode();
	board_cache_init();
}
#endif
#endif /* !CONFIG_SPL */

int dram_init(void)
{
	int i, j;
	size_t size;
	struct udevice *dev;
	struct usable_ram_partition_table *rpt;
	struct ram_partition_entry *rpe;

	uclass_get_device(UCLASS_SMEM, 0, &dev);
	rpt = smem_get(dev, -1, SMEM_USABLE_RAM_PARTITION_TABLE, &size);

	if (rpt == NULL)
		return -ENODEV;

	rpe = &rpt->ram_part_entry[0];

	for (j = i = 0; i < rpt->num_partitions; i++, rpe++)
		if ((rpe->partition_category == RAM_PARTITION_SDRAM) &&
			(rpe->partition_type == RAM_PARTITION_SYS_MEMORY))
			gd->ram_size += rpe->length;

	return 0;
}

#if !defined(CONFIG_SPL)
int dram_init_banksize(void)
{
	int i, j;
	size_t size;
	struct udevice *dev;
	struct usable_ram_partition_table *rpt;
	struct ram_partition_entry *rpe;

	uclass_get_device(UCLASS_SMEM, 0, &dev);
	rpt = smem_get(dev, -1, SMEM_USABLE_RAM_PARTITION_TABLE, &size);

	if (rpt == NULL)
		return -ENODEV;

	rpe = &rpt->ram_part_entry[0];

	for (j = i = 0; i < rpt->num_partitions; i++, rpe++) {
		if ((rpe->partition_category == RAM_PARTITION_SDRAM) &&
			(rpe->partition_type == RAM_PARTITION_SYS_MEMORY)) {
			gd->bd->bi_dram[i].start = rpe->start_address;
			gd->bd->bi_dram[i].size = rpe->length;
		}
	}

	return 0;
}
#endif /* CONFIG_SPL */

int ft_board_setup(void *blob, struct bd_info __maybe_unused *bd)
{
	ipq_ft_board_setup(blob, bd);

	return 0;
}

int board_early_init_f(void)
{
#if defined(CONFIG_SMEM) && defined(CONFIG_MSM_SMEM)
	size_t size;
	struct udevice *dev;
	union ipq_platform *platform_type;

	uclass_get_device(UCLASS_SMEM, 0, &dev);
	platform_type = smem_get(dev, -1, SMEM_HW_SW_BUILD_ID, &size);
	if (IS_ERR_OR_NULL(platform_type)) {
		debug("Failed to get SMEM item: SMEM_HW_SW_BUILD_ID\n");
		return -ENODEV;
	}

	g_board_machid = ((platform_type->v1.hw_platform << 24) |
				((SOCINFO_VERSION_MAJOR(
				platform_type->v1.platform_version)) << 16) |
				((SOCINFO_VERSION_MINOR(
				platform_type->v1.platform_version)) << 8) |
				(platform_type->v1.hw_platform_subtype));
#endif
	/*
	 * SoC specific early init
	 */
	ipq_board_early_init_f();

	return 0;
}

#if defined(CONFIG_ARM64) && defined(CFG_EMUL_FREQUENCY_DIVIDER)
void setup_arch_cntfreq(void)
{
	unsigned long freq = CONFIG_COUNTER_FREQUENCY /
				CFG_EMUL_FREQUENCY_DIVIDER;
	asm volatile("msr cntfrq_el0, %0" : : "r" (freq) : "memory");

        return;
}
#endif

int board_init(void)
{
	/*
	 * create device pointer
	 */
	ipq_bdinfo = (struct ipq_board_info *)malloc_cache_aligned(
			sizeof(struct ipq_board_info));
	if (ipq_bdinfo == NULL) {
		printf("%s No emough Space\n", __func__);
		return -ENOMEM;
	}

	memset(ipq_bdinfo, 0, sizeof(struct ipq_board_info));

#if defined(CONFIG_SMEM) && defined(CONFIG_MSM_SMEM)
	ipq_board_read_smem_info(ipq_bdinfo);
#endif

#if defined(CONFIG_ARM64) && defined(CFG_EMUL_FREQUENCY_DIVIDER)
	struct ipq_smem_flash_info *sfi = ipq_get_smem_info();
	if (sfi && (current_el() == 3) && (sfi->flash_type != SMEM_BOOT_NO_FLASH))
		setup_arch_cntfreq();
#endif
	/*
	 * update Global bdinfo table
	 */
	gd->bd->bi_boot_params = BOOT_PARAMS_ADDR;

	return 0;
}

__weak void ipq_update_comm_type(void) {}

#if defined(CONFIG_BOARD_EARLY_INIT_R)
int board_early_init_r(void)
{
	ipq_update_sfi_block_size();
	/*
	 * Update env address in runtime , support only in Nor flash
	 */
	ipq_runtime_sf_env_update();

	ipq_update_lmb_reservation();

	ipq_update_comm_type();

	return 0;
}
#endif

#if defined(CONFIG_BOARD_LATE_INIT)
int board_late_init(void)
{
	ipq_board_late_init();

	return 0;
}
#endif

int mach_cpu_init(void)
{
#if CONFIG_IS_ENABLED(IPQ_DYNAMIC_RELOCATION) && defined(CONFIG_IPQ_CRASHDUMP)
	/*
	 * Set GD_FLG_SKIP_RELOC early (before arch_setup_dest_addr) so
	 * that gd->relocaddr is adjusted correctly for the in-place path.
	 * DLOAD_MAGIC_COOKIE in TCSR_BOOT_MISC indicates crashdump boot.
	 */
	if (ipq_read_tcsr_boot_misc() & DLOAD_MAGIC_COOKIE)
		gd->flags |= GD_FLG_SKIP_RELOC;
#else
	gd->flags |= GD_FLG_SKIP_RELOC;
#endif
	return 0;
}

int arch_setup_dest_addr(void)
{
#if CONFIG_IS_ENABLED(IPQ_DYNAMIC_RELOCATION)
	if (gd->flags & GD_FLG_SKIP_RELOC) {
		/* Run in-place: relocaddr = load address, reloc_off = 0 */
		gd->relocaddr = CONFIG_TEXT_BASE;
		gd->reloc_off = 0;
	} else {
		/*
		 * Reserve 2MB above the relocated U-Boot image for the
		 * noncached DMA region used by the network driver.
		 * SZ_2M (instead of SZ_1M) ensures nc_start =
		 * ALIGN(relocaddr + mon_len, SZ_1M) lands within the
		 * reserved gap and never falls exactly at a DDR bank
		 * boundary, avoiding an MMU block-split that would
		 * exceed the SZ_64K page-table budget.
		 *
		 * Layout after reserve_uboot() subtracts mon_len:
		 *   [gd->relocaddr]              ← relocated U-Boot image
		 *   [ALIGN(relocaddr+mon_len,1M)] ← noncached DMA region (1MB)
		 *   [gd->ram_top - 1MB]          ← unused gap (alignment slack)
		 *   [gd->ram_top]                ← top of RAM
		 */
		gd->relocaddr -= SZ_2M;
	}
#else
	gd->relocaddr = CONFIG_TEXT_BASE;
	gd->reloc_off = gd->relocaddr - CONFIG_TEXT_BASE;
#endif
	return 0;
}

int reserve_arch(void)
{
#if CONFIG_IS_ENABLED(IPQ_DYNAMIC_RELOCATION)
	bool want_skip = false;

#if defined(CONFIG_IPQ_CRASHDUMP)
	want_skip = !!(ipq_read_tcsr_boot_misc() & DLOAD_MAGIC_COOKIE);
#endif

	/*
	 * reserve_uboot() in board_f.c sets GD_FLG_SKIP_RELOC when
	 * CONFIG_SKIP_RELOCATE=y (compile-time, cannot be prevented).
	 * Re-evaluate the runtime condition here — after reserve_uboot()
	 * but before setup_reloc() — to handle both defconfig cases:
	 *
	 * Case A: CONFIG_SKIP_RELOCATE=y, normal boot (want_skip=false)
	 *   reserve_uboot() set the flag but we want relocation.
	 *   Clear the flag and subtract mon_len from gd->relocaddr
	 *   (reserve_uboot() skipped this subtraction because the flag
	 *   was set). arch_setup_dest_addr() already subtracted SZ_1M,
	 *   so the DMA region remains correctly placed.
	 *
	 * Case B: CONFIG_SKIP_RELOCATE=y, crashdump (want_skip=true)
	 *   mach_cpu_init() already set the flag; reserve_uboot() is a
	 *   no-op. No change needed.
	 *
	 * Case C: CONFIG_SKIP_RELOCATE=n, crashdump (want_skip=true)
	 *   mach_cpu_init() set the flag; reserve_uboot() did not touch
	 *   it. No change needed.
	 *
	 * Case D: CONFIG_SKIP_RELOCATE=n, normal boot (want_skip=false)
	 *   Neither mach_cpu_init() nor reserve_uboot() set the flag.
	 *   No change needed.
	 */
	if (!want_skip && (gd->flags & GD_FLG_SKIP_RELOC)) {
		/* Case A: undo the compile-time skip, enable relocation */
		gd->flags &= ~GD_FLG_SKIP_RELOC;
		gd->relocaddr -= gd->mon_len;
		gd->relocaddr &= ~(4096 - 1);
		gd->start_addr_sp = gd->relocaddr;
	}
#endif /* CONFIG_IS_ENABLED(IPQ_DYNAMIC_RELOCATION) */

	return 0;
}

#if defined(CONFIG_SPL)
/**
 * arm_reserve_mmu() - Reserve space for MMU page tables.
 *
 * This function reserves memory for the MMU page tables, ensuring that
 * sufficient space is allocated for the translation tables required by
 * the MMU. The reserved space is stored in global data structures for later use.
 *
 * Returns:
 *	0 on success, negative error code on failure.
 */
int arm_reserve_mmu(void)
{
#if !(CONFIG_IS_ENABLED(SYS_ICACHE_OFF) && CONFIG_IS_ENABLED(SYS_DCACHE_OFF))
	/* reserve TLB table */
	gd->arch.tlb_size = PGTABLE_SIZE;

	gd->arch.tlb_addr = (unsigned long)(memalign(SZ_4K,
							gd->arch.tlb_size));
	if (!gd->arch.tlb_addr) {
		pr_err("%s No enough Space for pagetable\n", __func__);
		return -ENOMEM;
	}

#endif

	return 0;
}
#else
int arm_reserve_mmu(void)
{
#if !(CONFIG_IS_ENABLED(SYS_ICACHE_OFF) && CONFIG_IS_ENABLED(SYS_DCACHE_OFF))
	/* reserve TLB table */
	gd->arch.tlb_size = PGTABLE_SIZE;

	if (gd->flags & GD_FLG_SKIP_RELOC) {
		/*
		 * In-place (crashdump) path: TLB immediately after the
		 * U-Boot image at its load address, 64KB aligned.
		 */
		gd->arch.tlb_addr = CONFIG_TEXT_BASE + gd->mon_len;
		gd->arch.tlb_addr += (0x10000 - 1);
		gd->arch.tlb_addr &= ~(0x10000 - 1);
	} else {
		/*
		 * Normal reloc path: replicate the generic arm_reserve_mmu()
		 * behaviour from arch/arm/lib/cache.c — subtract tlb_size
		 * from gd->relocaddr, align to 64KB, and place TLB there.
		 * reserve_uboot() will later subtract mon_len from
		 * gd->relocaddr to place the U-Boot image below the TLB.
		 */
		gd->relocaddr    -= gd->arch.tlb_size;
		gd->relocaddr    &= ~(0x10000 - 1);
		gd->arch.tlb_addr = gd->relocaddr;
	}

	debug("TLB table from %08lx to %08lx\n", gd->arch.tlb_addr,
	      gd->arch.tlb_addr + gd->arch.tlb_size);

#ifdef CFG_SYS_MEM_RESERVE_SECURE
	/*
	 * Record allocated tlb_addr in case gd->tlb_addr is overwritten
	 * with a location within secure RAM.
	 */
	gd->arch.tlb_allocated = gd->arch.tlb_addr;
#endif

	if (IS_ENABLED(CONFIG_CMO_BY_VA_ONLY)) {
		/*
		 * Ensure page tables are in a valid state before
		 * invalidate_dcache_all() is called prior to mmu_setup().
		 */
		memset((void *)gd->arch.tlb_addr, 0, gd->arch.tlb_size);
	}
#endif
	return 0;
}
#endif

#ifdef CONFIG_DTB_RESELECT
int embedded_dtb_select(void)
{
	int rescan, i;
	struct multidtb_config *dtb = g_board_dtb_info;

	for (i = 0; i < dtb->ncount; ++i) {
		if (dtb->list[i].machid == g_board_machid) {
			strlcpy(dtb->dts_base, dtb->list[i].dts,
				BOARD_DTS_MAX_NAMELEN);
			dtb->index = i;
			break;
		}
	}

	ipq_update_board_name(g_board_machid, dtb);

	fdtdec_resetup(&rescan);

	return 0;
}
#endif /* CONFIG_DTB_RESELECT */

#if defined(CONFIG_SPL)
#if defined(CONFIG_TARGET_IPQ5210)
bool hermosa_prefer_mbn_v7(void)
{
	static bool decision_valid;
	static bool prefer_v7;
	uint32_t hw_version;
	uint32_t major;
	uint32_t minor;

	if (decision_valid)
		return prefer_v7;

	hw_version = ipq_get_soc_hw_version();
	major = TCSR_SOC_HW_VERSION_MAJOR(hw_version);
	minor = TCSR_SOC_HW_VERSION_MINOR(hw_version);
	prefer_v7 = ((major << 8) | minor) < HERMOSA_HW_VERSION_1_2;

	printf("TCSR_SOC_HW_VERSION=0x%08x (%u.%u)\n",
	       hw_version, major, minor);
	printf("%s\n", prefer_v7 ? "Preferring MBN v7 FIT configuration" :
				    "Using default FIT configuration");
	decision_valid = true;

	return prefer_v7;
}
#endif

int board_fit_config_name_match(const char *name)
{
	/*
	 * SPL loads the pre-HLOS images from bootldr FIT image
	 * as below
	 *
	 * In borad_init_f() - Matches "pre-ddr" configuration node and
	 * load the images mentioned in its <loadables>
	 *
	 * In borad_init_r() - Matches "post-ddr" configuration node and
	 * load the images mentioned in its <loadables>
	 *
	 * On hermosa (IPQ5210) only, if the chip's TCSR_SOC_HW_VERSION is
	 * below 1.2, the FIT's MBN v7 dual-signed configuration nodes
	 * ("pre-ddr-v7" / "post-ddr-v7") are preferred when present. Since
	 * template.its orders the v7 nodes before their default
	 * counterparts, matching both here means the v7 node wins if
	 * present; if the FIT doesn't contain one (e.g. an older/
	 * non-dual-signed image), the scan simply continues to the plain
	 * node instead - no separate fallback logic is needed. All other
	 * targets, and hermosa silicon >= 1.2, match only the plain node,
	 * exactly as before this change.
	 */
	bool match_v7 = false;

#if defined(CONFIG_TARGET_IPQ5210)
	match_v7 = hermosa_prefer_mbn_v7();
#endif

	if (!(gd->flags & GD_FLG_SPL_INIT)) {
		if ((match_v7 && !strcmp(name, "pre-ddr-v7")) ||
		    !strcmp(name, "pre-ddr")) {
			printf("Selected FIT Config: %s\n", name);
			return 0;
		}
	} else {
		if ((match_v7 && !strcmp(name, "post-ddr-v7")) ||
		    !strcmp(name, "post-ddr")) {
			printf("Selected FIT Config: %s\n", name);
			return 0;
		}
	}

	return -EINVAL;
}
#else
#ifdef CONFIG_MULTI_DTB_FIT
int board_fit_config_name_match(const char *name)
{
	struct multidtb_config *dtb = g_board_dtb_info;
	char *suffix = strstr(name, DTS_SUFFIX);
	size_t len, base_len = strlen(dtb->dts_base);

	if (suffix)
		len = suffix - name;
	else
		len = strlen(name);

	if (len == base_len) {
		if (!strncmp(name, dtb->dts_base, len)) {
			printf("Booting %s\n", dtb->dts_name);
			return 0;
		}
	}

	return -1;
}
#endif /* CONFIG_MULTI_DTB_FIT */
#endif /* CONFIG_SPL */

/*
 * Flush range from all levels of d-cache/unified-cache.
 * Affects the range,
 *      if cache is algined,
 *              from : start
 *              to   : start + size - 1
 *      if cache is not aligned,
 *              from : start - cache aligne address
 *              to   : start + size - 1 + cache aligne address
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

#if defined(CONFIG_SPL)
int fdtdec_board_setup(const void *fdt_blob)
{
	return 0;
}
#else
int fdtdec_board_setup(const void *fdt_blob)
{
	return ipq_uboot_fdt_fixup((void*)fdt_blob, UBOOT_FIXUP_SMEM);
}
#endif

#ifdef CONFIG_OF_BOARD_FIXUP
int board_fix_fdt(void *rw_fdt_blob)
{
	ipq_uboot_fdt_fixup(rw_fdt_blob, UBOOT_FIXUP_SMEM);
	ipq_uboot_fdt_fixup(rw_fdt_blob, UBOOT_FIXUP_USB);
#ifdef CONFIG_BOOT_BANK_FIXUP
	ipq_uboot_fdt_fixup(rw_fdt_blob, UBOOT_FIXUP_BOOTED_BANK);
#endif

	return 0;
}
#endif

phys_size_t get_effective_memsize(void)
{
	phys_size_t ram_size = min(gd->ram_size,
				(phys_size_t)CFG_SYS_SDRAM_BASE0_SIZE);

#ifndef CONFIG_ARM64
	if (((uint64_t)gd->ram_base + ram_size) > ULONG_MAX)
		ram_size = ULONG_MAX - gd->ram_base;
#endif
	return ram_size;
}

#ifdef CONFIG_ARM64
/*
 * board_get_usable_ram_top() - Return the top of usable RAM for ARM64.
 *
 * On ARM64 IPQ platforms DDR may be split across multiple non-contiguous
 * banks. The number of banks, their base addresses and sizes vary per SoC
 * and board configuration and are reported at runtime via the SMEM usable
 * RAM partition table.
 *
 * get_effective_memsize() caps ram_top to the first bank so that U-Boot
 * is placed in a contiguous region. For ARM64 U-Boot can address the full
 * 64-bit space, so ram_top should be the end of the highest DDR bank.
 * This prevents lmb_add_memory() from reserving higher banks wholesale
 * as no-overwrite.
 *
 * dram_init_banksize() runs after setup_dest_addr() so gd->bd->bi_dram[]
 * is not yet populated here. Read the SMEM usable-RAM partition table
 * directly — the same source used by dram_init().
 */
phys_addr_t board_get_usable_ram_top(phys_size_t total_size)
{
	size_t size;
	struct udevice *dev;
	struct usable_ram_partition_table *rpt;
	struct ram_partition_entry *rpe;
	phys_addr_t top = 0;
	int i;

	uclass_get_device(UCLASS_SMEM, 0, &dev);
	rpt = smem_get(dev, -1, SMEM_USABLE_RAM_PARTITION_TABLE, &size);

	if (rpt == NULL)
		return gd->ram_top;

	rpe = &rpt->ram_part_entry[0];
	for (i = 0; i < rpt->num_partitions; i++, rpe++) {
		if ((rpe->partition_category == RAM_PARTITION_SDRAM) &&
		    (rpe->partition_type == RAM_PARTITION_SYS_MEMORY)) {
			phys_addr_t bank_end = rpe->start_address + rpe->length;

			if (bank_end > top)
				top = bank_end;
		}
	}

	return top ? top : gd->ram_top;
}
#endif

#if CONFIG_IS_ENABLED(NAND_QTI)
void board_nand_init(void)
{
	struct udevice *dev;
	int ret;

	struct ipq_smem_flash_info *sfi = ipq_get_smem_info();

	/*
	 * Since the training partition info present in the gpt table
	 * which resides inside SPI-NOR flash so spi nor probe is must
	 * before nand init in NOTGPT case.
	 */

	if (sfi && sfi->flash_type == SMEM_BOOT_NORGPT_FLASH) {
#ifdef CONFIG_IPQ_SPI_NOR
		ipq_spi_probe();
#endif
	}

	ret = uclass_get_device_by_driver(UCLASS_MTD,
					  DM_DRIVER_GET(qti_nand), &dev);
	if (ret && ret != -ENODEV)
		pr_err("Failed to initialize %s. (error %d)\n",
		       dev->name, ret);
}
#endif
