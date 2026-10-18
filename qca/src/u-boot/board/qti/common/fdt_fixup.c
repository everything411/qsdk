// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2015-2017, 2020 The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <common.h>
#include <asm/io.h>
#include <asm/global_data.h>
#include <jffs2/load_kernel.h>
#include <env.h>
#include <fdt_support.h>
#include <fdtdec.h>
#include <stdlib.h>
#include <mtd_node.h>
#include <linux/mtd/mtd.h>
#include <nand.h>
#include <memalign.h>
#include <linux/ctype.h>
#ifdef CONFIG_IPQ_SPI_NOR
#include <spi.h>
#include <spi_flash.h>
#endif
#include "ipq_board.h"

DECLARE_GLOBAL_DATA_PTR;

#ifndef NAND_MTDIDS
#define NAND_MTDIDS		"nand0=nand0"
#endif
#ifndef NOR_MTDIDS
#define NOR_MTDIDS		"nor0=spi0.0"
#endif
#ifndef NORPLUSNAND_MTDIDS
#define NORPLUSNAND_MTDIDS	"nand0=nand0,nor0=spi0.0"
#endif

typedef void (*fdt_fixup_t)(void *blob);

#define FDT_EDIT "fdtedit"
/* Buffer size to hold numbers from 0-99 + 1 NULL character */
#define NUM_BUF_SIZE 3

/* setenv fdteditnum <num>   - here <num> represents number of envs to parse
 * Note: without setting 'fdteditnum' fdtedit envs will not parsed
 *
 * fdtedit<num> <node>%<property>%<node_value>   - dts patching env format
 * here '%' is separator; <num> can be between 1 to 99;
 *
 * 1. To change add/change a particular property of a node:
 *       setenv fdtedit0 <node_path>%<property>%<value>
 *
 *    This can be used to add properties which doesn't have any value
 *    associated.
 *       eg: qca,secure; property of q6v5_wcss@CD00000 node can be added as:
 *       setenv fdtedit0 /soc/q6v5_wcss@CD00000/%qca,secure%1
 *    other eg:
 *       fdtedit0=/soc/q6v5_wcss@CD00000%qca,sec-reset-cmd%0x19
 *       fdtedit1=/soc/usb3@8A00000/dwc3@8A00000%dr_mode%?peripheral
 *       fdtedit2=/soc/qcom,gadget_diag@0/%status%?ok
 *
 * 2. To delete a property of a node:
 *       setenv fdtedit0 <node_path>%delete%<property>
 *    example:
 *       fdtedit0=/soc/q6v5_wcss@CD00000%delete%?qca,secure
 *
 * The last param in both add or delete case, if it is a string, it should
 * start with '?' else if it is a number, it can be put directly.
 * check above examples for reference.
 *
 * 3. To add 32bit or 64bit array values:
 *       setenv fdtedit0 <node_path>%<bit_value>?<num_values>?
 *			<property_name>%<value1>?<value2>?<value3>?..
 *       <bit_value> can be 32 / 64;  <num_values> is number of array elements
 *       to be patched; <property_name> is the actual name of the property to
 *       be patched; each array value has to be separated by '?'
 *       for reg = <addr> <size>; <num_values> is 2 in this case
 *    example:
 *       setenv fdtedit0 /soc/dbm@0x8AF8000/%32?2?reg%0x8AF8000?0x500
 *       setenv fdtedit1 /soc/pci@20000000/%32?2?bus-range%0xee?0xee
 *       setenv fdtedit2
 *		/soc/usb3@8A00000/%32?4?reg%0x8AF8600?0x200?0x8A00000?0xcb00
 *       setenv fdtedit3
 *		/reserved-memory/tzapp@49B00000/%64?2?reg%0x49A00000?0x500000
 */
void parse_fdt_fixup(char* buf, void *blob)
{
	int nodeoff, value, num_values, i;
	char *node, *property, *node_value, *sliced_string;
	bool if_string = true, bit32 = true;
	u32 *values32;
	u64 *values64;
	int ret = 0;

	/* env is split into <node>%<property>%<node_value>. '%' is separator*/
	node = strsep(&buf, "%");
	if(!node) {
		ret = EINVAL;
	}

	property = strsep(&buf, "%");
	if(!property) {
		ret = EINVAL;
	}

	node_value = strsep(&buf, "%");
	if(!node_value) {
		ret = EINVAL;
	}

	if(ret == EINVAL)
		goto arg_err;

	debug("node: %s  property: %s  node_value: %s\n", node,
			property, node_value);

	/* if '?' is present then node_value is string;
	 * else, node_value is 32bit value
	 */
	if (node_value && node_value[0] != '?') {
		if_string = false;
		value = simple_strtoul(node_value, NULL, 10);
	} else {
		/* skip '?' */
		node_value++;
	}

	nodeoff = fdt_path_offset(blob, node);
	if (nodeoff < 0) {
		printf("%s: unable to find node '%s'\n", __func__, node);
		return;
	}

	if (!strncmp(property, "delete", strlen("delete"))) {
		/* handle property deletes */
		ret = fdt_delprop(blob, nodeoff, node_value);
		if (ret) {
			printf("%s: unable to delete %s\n",
				__func__, node_value);
			return;
		}
	} else if (!strncmp(property, "32", strlen("32")) ||
			!strncmp(property, "64", strlen("64"))) {
		/* if property name starts with '32' or '64', then it is used
		 * for patching array of 32bit / 64bit values correspondingly.
		 * 32bit patching is usually used to patch reg = <addr> <size>;
		 * but could also be used to patch multiple addresses & sizes
		 * <property> = <addr1> <size1> <addr2> <size2> ..
		 * 64bit patching is usually used to patch reserved
		 * memory nodes
		 */
		sliced_string = strsep(&property, "?");
		if(sliced_string) {

			if (simple_strtoul(sliced_string, NULL, 10) == 64)
				bit32 = false;

			/* get the number of array values */
			sliced_string = strsep(&property, "?");
			if(sliced_string) {
				num_values = simple_strtoul(sliced_string,
								NULL, 10);

				if (bit32 == true) {
					values32 = malloc(num_values * \
								sizeof(u32));
					if(!values32) {
						printf("%s: Unable to allocate"
							" memory\n", __func__);
						return;
					}

					for (i = 0; i < num_values; i++)  {
						sliced_string = strsep(
							&node_value, "?");
						if(!sliced_string) {
							ret = -EINVAL;
							goto arg_err;
						}

						values32[i] =  cpu_to_fdt32(
								simple_strtoul(
								sliced_string,
								NULL, 10));
					}

					ret = fdt_setprop(blob, nodeoff,
							property, values32,
							num_values * \
								sizeof(u32));
					if (ret) {
						printf("%s: failed to set"
								" prop %s\n",
								__func__,
								property);
						return;
					}
				} else {
					values64 = malloc(num_values * \
								sizeof(u64));
					if(!values64) {
						printf("%s: Unable to"
							" allocate memory\n",
							__func__);
						return;
					}

					for (i = 0; i < num_values; i++)  {
						sliced_string = strsep(
								&node_value,
									"?");
						if(!sliced_string) {
							ret = -EINVAL;
							goto arg_err;
						}

						values64[i] = cpu_to_fdt64(
								simple_strtoul(
								sliced_string,
								NULL, 10));
					}

					ret = fdt_setprop(blob, nodeoff,
							property, values64,
							num_values * \
								sizeof(u64));
					if (ret) {
						printf("%s: failed to"
							" set prop %s\n",
							__func__,
							property);
						return;
					}
				}
			} else {
				ret = EINVAL;
			}
		} else {
			ret = EINVAL;
		}

	} else if (!if_string) {
		/* handle 32bit integer value patching */
		ret = fdt_setprop_u32(blob, nodeoff, property, value);
		if (ret) {
			printf("%s: failed to set prop %s\n",
					__func__, property);
			return;
		}
	} else {
		/* handle string value patching
		 * usually used to patch status = "ok"; status = "disabled";
		 */
		ret = fdt_setprop(blob, nodeoff, property,
				node_value,
				(strlen(node_value) + 1));
		if (ret) {
			printf("%s: failed to set prop %s\n",
					__func__, property);
			return;
		}
	}


arg_err:
	if(ret == EINVAL)
		printf("%s: invalid string\n", __func__);
	return;
}

/* check parse_fdt_fixup for detailed explanation */
static void ipq_fdt_fixup(void *blob)
{
	int i, fdteditnum;
	char buf[sizeof(FDT_EDIT) + NUM_BUF_SIZE], num[NUM_BUF_SIZE];
	char *s;

	/* fdteditnum - defines the number of envs to parse
	 * starting from 0. eg: fdtedit0, fdtedit1, and so on.
	 */
	s = env_get("fdteditnum");
	if (s)
		fdteditnum = simple_strtoul(s, NULL, 10);
	else
		return;

	printf("%s: fixup fdtedits\n", __func__);

	for (i = 0; i <= fdteditnum; i++) {
		/* Generate env names fdtedit0, fdtedit1,..fdteditn */
		strlcpy(buf, FDT_EDIT, sizeof(buf));
		snprintf(num, sizeof(num), "%d", i);
		strlcat(buf, num, sizeof(buf));

		s = env_get(buf);
		if (s)
			parse_fdt_fixup(s, blob);
	}
}

__weak void ipq_fdt_fixup_flash(void *blob)
{
#ifdef CONFIG_MMC
	uint32_t flash_type = gd->board_type & FLASH_TYPE_MASK;
	int nand_nodeoff = -EINVAL;
	int mmc_nodeoff = -EINVAL;
#ifdef LINUX_6_x_NAND_DTS_NODE
	nand_nodeoff = fdt_path_offset(blob, LINUX_6_x_NAND_DTS_NODE);
#endif
#ifdef LINUX_6_x_MMC_DTS_NODE
	mmc_nodeoff = fdt_path_offset(blob, LINUX_6_x_MMC_DTS_NODE);
#endif
	if (flash_type == SMEM_BOOT_NORPLUSEMMC ||
		flash_type == SMEM_BOOT_MMC_FLASH ) {
		if ((nand_nodeoff > 0) && (mmc_nodeoff > 0)) {
#ifdef LINUX_6_x_MMC_DTS_NODE
			parse_fdt_fixup(LINUX_6_x_MMC_DTS_NODE"%"STATUS_OK,
					blob);
#endif
#ifdef LINUX_6_x_NAND_DTS_NODE
			parse_fdt_fixup(
				LINUX_6_x_NAND_DTS_NODE"%"STATUS_DISABLED,
				blob);
#endif
		} else {
#ifdef LINUX_5_4_NAND_DTS_NODE
			nand_nodeoff = fdt_path_offset(blob,
					LINUX_5_4_NAND_DTS_NODE);
#endif
#ifdef LINUX_5_4_MMC_DTS_NODE
			mmc_nodeoff = fdt_path_offset(blob,
					LINUX_5_4_MMC_DTS_NODE);
#endif
			if ((nand_nodeoff <= 0) || (mmc_nodeoff <= 0))
				return;
#ifdef LINUX_5_4_MMC_DTS_NODE
			parse_fdt_fixup(LINUX_5_4_MMC_DTS_NODE"%"STATUS_OK,
					blob);
#endif
#ifdef LINUX_5_4_NAND_DTS_NODE
			parse_fdt_fixup(
				LINUX_5_4_NAND_DTS_NODE"%"STATUS_DISABLED,
				blob);
#endif
		}
	}
#endif
	return;
}

__weak void ipq_fdt_fixup_smem(void *blob)
{
	return;
}

__weak void ipq_fdt_fixup_atf(void *blob)
{
	return;
}

__weak void ipq_fdt_fixup_socinfo(void *blob)
{
	uint32_t cpu_type;
	int nodeoff, ret;
	socinfo_t *ipq_socinfo = get_socinfo();

	nodeoff = fdt_path_offset(blob, "/");

	if (nodeoff < 0) {
		printf("ipq: fdt fixup cannot find root node\n");
		return;
	}

	ret = fdt_setprop(blob, nodeoff, "cpu_type",
			  (void*)&ipq_socinfo->cpu_type, sizeof(cpu_type));
	if (ret)
		printf("%s: cannot set cpu type %d\n", __func__, ret);

	ret = fdt_setprop(blob, nodeoff, "soc_version_major",
			 (void*) &ipq_socinfo->soc_version_major,
			  sizeof(ipq_socinfo->soc_version_major));
	if (ret)
		printf("%s: cannot set soc_version_major %d\n",
		       __func__, ipq_socinfo->soc_version_major);

	ret = fdt_setprop(blob, nodeoff, "soc_version_minor",
			  (void*)&ipq_socinfo->soc_version_minor,
			  sizeof(ipq_socinfo->soc_version_minor));
	if (ret)
		printf("%s: cannot set soc_version_minor %d\n",
		       __func__, ipq_socinfo->soc_version_minor);
	return;
}

__weak void ipq_fdt_fixup_board(void *blob)
{
	return;
}

#if defined(CONFIG_EFI_PARTITION)
void ipq_memcpy16(u16 *src, u8 *dst, int size)
{
	for (int i = 0; i < size; ++i)
		dst[i] = src[i] & 0xFF;
}

int validate_and_copy(gpt_entry *pte, struct smem_ptn *p)
{
	u8 *guid;

	if (!pte || !p)
		return 1;

	guid = pte->partition_type_guid.b;

	while (*guid != '\0') {
		if ((*guid - 0) != 0)
			break;
		++guid;
	}

	if (*guid == '\0')
		return 1;

	p->start = (lbaint_t)le64_to_cpu(pte->starting_lba);

	p->size = (lbaint_t)le64_to_cpu(pte->ending_lba) + 1 - p->start;

	ipq_memcpy16(pte->partition_name, p->name, SMEM_PTN_NAME_MAX);

	return 0;
}
#endif
#ifdef CONFIG_FDT_FIXUP_PARTITIONS
void ipq_smem_part_to_mtdparts(char *mtdid, int len)
{
	ipq_smem_flash_info_t *sfi = get_ipq_smem_flash_info();
	int i, ret, ncount = 0;
	int device_id = 0;
	char *part = mtdid, *unit;
	int init = 0;
	uint32_t bsize;
	struct smem_ptable *ptable = get_ipq_part_table_info();
	struct smem_ptn *p;
	loff_t psize;
	int isnand =  0;
#if defined(CONFIG_NOR_BLK)
	struct smem_ptn sp;
	struct blk_desc *dev = NULL;
#if defined(CONFIG_EFI_PARTITION)
	gpt_entry *gpt_pte;
#endif
#endif
#ifdef CONFIG_CMD_NAND
	struct mtd_info *mtd = get_nand_dev_by_index(0);
	if(!mtd) {
		printf("%s: mtd device not found\n", __func__);
		return;
	}
#endif
#if defined(CONFIG_NOR_BLK)
	if (sfi->flash_type == SMEM_BOOT_NORGPT_FLASH) {
		dev = blk_get_devnum_by_uclass_id(UCLASS_SPI, 0);
		if (!dev) {
			printf("No SPIBLK device found \n");
			return;
		}

#if defined(CONFIG_EFI_PARTITION)
		if (dev) {
			gpt_pte = get_gpt_entry(dev);
			if (!gpt_pte) {
				printf("Failed to get gpt table entry\n");
				return;
			} else {
				ncount = sfi->nor_gpt_pte.ncount;
			}
		}
#endif
		bsize = dev->blksz;
	}
#endif
	ret = snprintf(part, len, "%s:", mtdid);
	part += ret;
	len -= ret;

	if (sfi->flash_type != SMEM_BOOT_NORGPT_FLASH)
		ncount = ptable->len;

	for (i = 0; i < ncount && len > 0; i++) {
#if defined(CONFIG_NOR_BLK)
		if (sfi->flash_type == SMEM_BOOT_NORGPT_FLASH) {
#if defined(CONFIG_EFI_PARTITION)
			if (validate_and_copy(&gpt_pte[i], &sp))
				continue;
			isnand = gpt_find_which_flash(&gpt_pte[i]);
#endif
			p = &sp;
		} else
#endif
		{
			p = &ptable->parts[i];
			bsize = get_part_block_size(p, sfi);
			isnand = part_which_flash(p);
		}

		if (isnand && init == 0) {
			device_id = 0;
			ret = snprintf(part, len, ";nand%d:", device_id);
			part += ret;
			len -= ret;
			init = 1;
		}

		if (p->size == (~0u)) {
			/*
			 * Partition size is 'till end of device', calculate
			 * appropriately
			 */
#ifdef CONFIG_CMD_NAND
			psize = (mtd->size - (((loff_t)p->start) * bsize));
#else
			psize = 0;
#endif
		} else {
			psize =  ((loff_t)p->size) * bsize;
		}
#if defined(CONFIG_NOR_BLK)
		if (sfi->flash_type == SMEM_BOOT_NORGPT_FLASH) {
			if (isnand) {
				if (((((loff_t)p->start) * bsize) + psize) >
					smem_get_flash_size(1))
					continue;
			} else {
				if (((((loff_t)p->start) * bsize) + psize) >
					smem_get_flash_size(0))
					continue;
			}
		} else
#endif
			if (is_smem_part_exceed_flash_size(p,
				((((loff_t)p->start) * bsize) + psize)))
				continue;

		if ((psize > SZ_1M) && (((psize & (SZ_1M - 1)) == 0))) {
			psize /= SZ_1M;
			unit = "M@";
		} else if ((psize > SZ_1K) && (((psize & (SZ_1K - 1)) == 0))) {
			psize /= SZ_1K;
			unit = "K@";
		} else {
			unit = "@";
		}

		ret = snprintf(part, len, "%lld%s0x%llx(%s),", psize, unit,
				((loff_t)p->start) * bsize, p->name);
		part += ret;
		len -= ret;
	}

	if (i == 0)
		*mtdid = '\0';

	*(part-1) = 0;	/* Remove the trailing ',' character */
}

static int ipq_fdt_fixup_spi_nor_params(void *blob,
		const struct node_info *node_info, int node_info_size)
{
        int ret, nodeoff = -1;
        uint32_t val, i;
	ipq_smem_flash_info_t *sfi = get_ipq_smem_flash_info();
#if defined(CONFIG_NOR_BLK)
	struct spi_flash *flash = ipq_spi_probe();
	if (flash == NULL && sfi->flash_type == SMEM_BOOT_NORGPT_FLASH) {
		printf("Spi nor not found \n");
		return -1;
	}
#endif

	for (i = 0; i < node_info_size; i++) {
		if (node_info[i].type != MTD_DEV_TYPE_NOR)
			continue;

	        nodeoff = fdt_node_offset_by_compatible(blob, -1,
				node_info[i].compat);
		if (nodeoff >= 0)
			break;
        }

	if (nodeoff < 0) {
		printf("fdt-fixup: unable to find compatible node\n");
		return nodeoff;
	}

#if defined(CONFIG_NOR_BLK)
	if (sfi->flash_type == SMEM_BOOT_NORGPT_FLASH)
		val = cpu_to_fdt32(flash->sector_size);
	else
#endif
		val = cpu_to_fdt32(sfi->flash_block_size);

	ret = fdt_setprop(blob, nodeoff, "sector-size",
			&val, sizeof(uint32_t));
	if (ret) {
		printf("fdt-fixup: unable to set sector size(%d)\n", ret);
		return ret;
	}

#if defined(CONFIG_NOR_BLK)
	if (sfi->flash_type == SMEM_BOOT_NORGPT_FLASH)
		val = cpu_to_fdt32(flash->size);
	else
#endif
		val = cpu_to_fdt32(sfi->flash_density);

	if (val != 0) {
		ret = fdt_setprop(blob, nodeoff, "density",
				&val, sizeof(uint32_t));
		if (ret) {
			printf("fdt-fixup: unable to set density(%d)\n", ret);
			return ret;
		}
	}

	return 0;
}

static int ipq_set_mtdids(uint32_t flash_type)
{
	if (strlen(CONFIG_MTDIDS_DEFAULT))
		return 0;

	switch (flash_type) {
	case SMEM_BOOT_QSPI_NAND_FLASH:
		return env_set("mtdids", NAND_MTDIDS);
	case SMEM_BOOT_SPI_FLASH:
	case SMEM_BOOT_NORGPT_FLASH:
	case SMEM_BOOT_NORPLUSEMMC:
		return env_set("mtdids", NOR_MTDIDS);
	case SMEM_BOOT_NORPLUSNAND:
		return env_set("mtdids", NORPLUSNAND_MTDIDS);
	default:
		printf("Invalid flash type : 0x%X\n", flash_type);
	}

	return -1;
}

static void ipq_fdt_fixup_mtdparts(void *blob)
{
	uint32_t flash_type = gd->board_type & FLASH_TYPE_MASK;
	char *parts;
	char parts_str[4096];
	char *mtdparts = NULL;
	char *addparts = NULL;
	int len = sizeof(parts_str);

	if (ipq_set_mtdids(flash_type)) {
		printf("Failed to set mtdids\n");
		return;
	}

	switch (flash_type) {
	case SMEM_BOOT_QSPI_NAND_FLASH:
		snprintf(parts_str, sizeof(parts_str), "mtdparts=nand0");
		break;
	case SMEM_BOOT_SPI_FLASH:
	case SMEM_BOOT_NORGPT_FLASH:
	case SMEM_BOOT_NORPLUSEMMC:
		/* NOR density & sector-size fix-up */
		ipq_fdt_fixup_spi_nor_params(blob, fnodes, *fnode_entires);
		snprintf(parts_str, sizeof(parts_str), "mtdparts=spi0.0");
		break;
	case SMEM_BOOT_NORPLUSNAND:
		/* NOR density & sector-size fix-up */
		ipq_fdt_fixup_spi_nor_params(blob, fnodes, *fnode_entires);
		snprintf(parts_str, sizeof(parts_str), "mtdparts=spi0.0");
		break;
	default:
		printf("mtdpart fixup failed\n");
	}

	mtdparts = parts_str;
	if (mtdparts) {
		ipq_smem_part_to_mtdparts(mtdparts,len);
		if (mtdparts[0] != '\0') {
			addparts = env_get("addmtdparts");
			if (addparts) {
				debug("addmtdparts = %s\n", addparts);
				strlcat(mtdparts, ",", sizeof(parts_str));
				strlcat(mtdparts, addparts, sizeof(parts_str));
			}
			debug("mtdparts = %s\n", mtdparts);
			env_set("mtdparts", mtdparts);
		}
	}

	parts = env_get("mtdparts");
	if (parts)
		fdt_fixup_mtdparts(blob, fnodes, *fnode_entires);
	else
		return;
}
#endif

#ifdef CONFIG_CMD_NAND
static void ipq_fdt_fixup_qti_nand(void *blob)
{
	int ret;
	char fixup_cfg[128] = { 0 };
	uint32_t offset, size;
	loff_t training_offset;

	ret = ipq_get_training_part_info(&offset, &size);
	if (ret) {
		printf("Serial Training part offset not found.\n");
		return;
	}

	training_offset = (loff_t)offset;
#ifdef LINUX_6_x_NAND_DTS_NODE
	if (fdt_path_offset(blob, LINUX_6_x_NAND_DTS_NODE) > 0)
		snprintf(fixup_cfg, sizeof(fixup_cfg), "%s%s%lld",
				LINUX_6_x_NAND_DTS_NODE,
				"%qcom,training_offset%", training_offset);
#endif
#ifdef LINUX_5_4_NAND_DTS_NODE
	if (fdt_path_offset(blob, LINUX_5_4_NAND_DTS_NODE) > 0)
		snprintf(fixup_cfg, sizeof(fixup_cfg), "%s%s%lld",
				LINUX_5_4_NAND_DTS_NODE,
				"%qcom,training_offset%", training_offset);
#endif

	if (fixup_cfg[0] != 0)
		parse_fdt_fixup(fixup_cfg, blob);
}
#endif /* CONFIG_CMD_NAND */

static void ipq_fdt_fixup_usb_dev_mode(void *blob)
{
	const char *usb_cfg = env_get("usb_mode");
	if (!usb_cfg)
		return;
	if (!strncmp(usb_cfg, "peripheral", sizeof("peripheral"))) {
#ifdef LINUX_6_x_USB_DTS_NODE
		if (fdt_path_offset(blob, LINUX_6_x_USB_DTS_NODE) > 0) {
#ifdef LINUX_6_x_USB_DR_MODE_FIXUP
			parse_fdt_fixup(LINUX_6_x_USB_DR_MODE_FIXUP, blob);
#endif
#ifdef LINUX_6_x_USB_MAX_SPEED_FIXUP
			parse_fdt_fixup(LINUX_6_x_USB_MAX_SPEED_FIXUP, blob);
#endif
		}
#endif

#ifdef LINUX_6_x_USB2_DTS_NODE
		if (fdt_path_offset(blob, LINUX_6_x_USB2_DTS_NODE) > 0) {
#ifdef LINUX_6_x_USB_DR_MODE_FIXUP
			parse_fdt_fixup(LINUX_6_x_USB2_DR_MODE_FIXUP, blob);
#endif
#ifdef LINUX_6_x_USB_MAX_SPEED_FIXUP
			parse_fdt_fixup(LINUX_6_x_USB2_MAX_SPEED_FIXUP, blob);
#endif
		}
#endif

#ifdef LINUX_5_4_USB_DTS_NODE
		if (fdt_path_offset(blob, LINUX_5_4_USB_DTS_NODE) > 0) {
#ifdef LINUX_5_4_USB_DR_MODE_FIXUP
			parse_fdt_fixup(LINUX_5_4_USB_DR_MODE_FIXUP, blob);
#endif
#ifdef LINUX_5_4_USB_MAX_SPEED_FIXUP
			parse_fdt_fixup(LINUX_5_4_USB_MAX_SPEED_FIXUP, blob);
#endif
		}
#endif
	}
}

__weak void ipq_fdt_fixup_sku_based_usb_config(void *blob)
{
	return;
}

__weak bool ipq_fdt_board_specific_skip(int type)
{
	bool isskip = false;

	switch (type) {
#ifdef CFG_NC_RESERVATION
	case SECURE_SYS_UPGRADE:
		isskip = true;
		break;
#endif
	default:
		break;
	}

	return isskip;
}

static void ipq_fdt_fixup_dload_disable(void *blob)
{
	int parentoff, nodeoff, ret, i;
	const char *del_node[] = {
		"uboot",
		"bootloader",
		"sbl",
		NULL
	};
	char * s = env_get("bootargs");

	if ((s == NULL) || (s[0] == '\0')) {
		return;
	} else {
		if(!strstr(s, "qcom_scm.download_mode=0"))
			return;
	}

	if ((gd->board_type & SECURE_BOARD) &&
		(ipq_fdt_board_specific_skip(SECURE_SYS_UPGRADE)))
			return;

	/* Reserve only the TZ and SMEM memory region and free the rest */
#ifdef LINUX_RSVD_MEM_DTS_NODE
	parentoff = fdt_path_offset(blob, LINUX_RSVD_MEM_DTS_NODE);
#else
	parentoff = -1;
#endif
	if (parentoff >= 0) {
		for (i = 0; del_node[i]; i++) {
			nodeoff = fdt_subnode_offset(blob, parentoff,
						     del_node[i]);
			if (nodeoff < 0) {
				debug("fdt-fixup: unable to findnode (%s)\n",
					del_node[i]);
				continue;
			}
			ret = fdt_del_node(blob, nodeoff);
			if (ret != 0)
				debug("fdt-fixup: unable to delete node" \
					       " (%s)\n", del_node[i]);
		}
	} else {
		debug("fdt-fixup: unable to find node \n");
	}
}

static const fdt_fixup_t fixup_functions[] = {
	ipq_fdt_fixup_socinfo,
	ipq_fdt_fixup_smem,
#ifdef CONFIG_FDT_FIXUP_PARTITIONS
	ipq_fdt_fixup_mtdparts,
#endif
	ipq_fdt_fixup_flash,
	ipq_fdt_fixup,
#ifdef CONFIG_CMD_NAND
	ipq_fdt_fixup_qti_nand,
#endif
	ipq_fdt_fixup_atf,
	ipq_fdt_fixup_usb_dev_mode,
	ipq_fdt_fixup_sku_based_usb_config,
	ipq_fdt_fixup_dload_disable,
	ipq_fdt_fixup_board,
	NULL
};

static void fix_in_seq(const fdt_fixup_t fixup_f[], void *blob)
{
	const fdt_fixup_t *fixup_ptr;
	for(fixup_ptr = fixup_f ; *fixup_ptr ; ++fixup_ptr) {
		(*fixup_ptr)(blob);
		watchdog_reset();
	}
	return;
}

int ft_board_setup(void *blob, struct bd_info *bd)
{
	fix_in_seq(fixup_functions, blob);
	return 0;
}
