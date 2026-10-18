// SPDX-License-Identifier: GPL-2.0+
/*
 * Qualcomm ICE (Inline Crypto Engine) Generic Driver
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Copyright (c) 2013-2019, The Linux Foundation. All rights reserved.
 * Copyright (c) 2019, Google LLC
 * Copyright (c) 2023, Linaro Limited
 *
 * This file provides generic ICE initialization and configuration
 * for Qualcomm platforms supporting ICE hardware crypto engine.
 */

#include <common.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/bitfield.h>
#include <linux/iopoll.h>
#include <mmc.h>
#include <sdhci.h>
#include <dm.h>
#include <dm/device.h>
#include <dm/uclass.h>
#include <dm/of_extra.h>
#include <clk.h>
#ifdef CONFIG_TARGET_IPQ9574
#include <dt-bindings/clock/gcc-ipq9574.h>
#elif defined(CONFIG_TARGET_IPQ5424)
#include <dt-bindings/clock/gcc-ipq5424.h>
#endif

#define QCOM_ICE_REG_VERSION                    0x0008
#define QCOM_ICE_REG_FUSE_SETTING               0x0010
#define QCOM_ICE_REG_BIST_STATUS                0x0070
#define QCOM_ICE_REG_ADVANCED_CONTROL           0x1000
#define QCOM_ICE_REG_TEST_BUS_CONTROL           0x1010

/* ICE Error Status Registers */
#define QCOM_ICE_REG_GENERAL_ERR_STTS           0x0040
#define QCOM_ICE_REG_INVALID_CCFG_ERR_STTS      0x0048

/* ICE Stream Error Syndrome Registers */
#define QCOM_ICE_REG_STREAM1_ERROR_SYNDROME1    0x0120
#define QCOM_ICE_REG_STREAM1_ERROR_SYNDROME2    0x0124
#define QCOM_ICE_REG_STREAM1_ERROR_SYNDROME3    0x0128
#define QCOM_ICE_REG_STREAM2_ERROR_SYNDROME1    0x0130
#define QCOM_ICE_REG_STREAM2_ERROR_SYNDROME2    0x0134
#define QCOM_ICE_REG_STREAM2_ERROR_SYNDROME3    0x0138

#define ICE_VERSION_MAJOR_MASK                  GENMASK(31, 24)
#define ICE_VERSION_MINOR_MASK                  GENMASK(23, 16)
#define ICE_VERSION_STEP_MASK                   GENMASK(15, 0)

/* ICE Fuse Setting Register Fields */
#define ICE_FUSE_SETTING_MASK                   BIT(0)
#define ICE_FORCE_HW_KEY0_SETTING_MASK          BIT(1)
#define ICE_FORCE_HW_KEY1_SETTING_MASK          BIT(2)

/* ICE BIST Status Register Fields */
#define ICE_BIST_STATUS_MASK                    GENMASK(31, 28)

/* ICE Advanced Control Register Fields */
#define ICE_SW_RST_EN                           BIT(0)

/* ICE HCI Register Definitions */
#define ICE_CQ_CAPABILITIES                     0x04
#define ICE_HCI_SUPPORT                         BIT(28)
#define ICE_CQ_CONFIG                           0x08
#define CRYPTO_GENERAL_ENABLE                   BIT(1)
#define ICE_NONCQ_CRYPTO_PARAMS                 0x70
#define ICE_NONCQ_CRYPTO_DUN                    0x74

/* HC Vendor Specific Register Definitions */
#define HC_VENDOR_SPECIFIC_FUNC4                0x260
#define DISABLE_CRYPTO                          BIT(15)
#define HC_VENDOR_SPECIFIC_ICE_CTRL             0x800

/* ICE HCI Parameter Masks and Offsets */
#define MASK_SDHCI_MSM_ICE_HCI_PARAM_CE         0x1
#define MASK_SDHCI_MSM_ICE_HCI_PARAM_CCI        0xff
#define OFFSET_SDHCI_MSM_ICE_HCI_PARAM_CE       8
#define OFFSET_SDHCI_MSM_ICE_HCI_PARAM_CCI      0

/* Timeout values */
#define ICE_BIST_TIMEOUT_US                     5000    /* 5ms */
#define ICE_RESET_TIMEOUT_US                    1000    /* 1ms */

/* ICE Advanced Control Register bit definitions */
#define ICE_ADV_CTRL_LOW_POWER_MODE        0x7000	/* Clock gating enable bits */
#define ICE_ADV_CTRL_PERF_OPTIMIZE        0xd807100	/* Performance optimization bits */

/* ICE Test Bus Control Register bit definitions */
#define ICE_TEST_BUS_SELECTOR_MASK        0x0FFFFFFF  /* Clear upper 4 bits */
#define ICE_TEST_BUS_REG_ENABLE            0x2

/* Global variable to store CMDQ base address */
static void __iomem *g_cmdq_base = NULL;

/**
 * qcom_ice_check_supported() - Check if ICE is supported
 * @ice_base: ICE register base address
 *
 * Returns: true if ICE is supported, false otherwise
 */
static bool qcom_ice_check_supported(void __iomem *ice_base)
{
	u32 regval = readl(ice_base + QCOM_ICE_REG_VERSION);
	int major = FIELD_GET(ICE_VERSION_MAJOR_MASK, regval);
	int minor = FIELD_GET(ICE_VERSION_MINOR_MASK, regval);
	int step = FIELD_GET(ICE_VERSION_STEP_MASK, regval);

	if (major != 3 && major != 4) {
		printf("ICE: Unsupported version: v%d.%d.%d\n",
		       major, minor, step);
		return false;
	}

	debug("ICE: Found QC Inline Crypto Engine (ICE) v%d.%d.%d\n",
	      major, minor, step);

	regval = readl(ice_base + QCOM_ICE_REG_FUSE_SETTING);
	if (regval & (ICE_FUSE_SETTING_MASK |
		      ICE_FORCE_HW_KEY0_SETTING_MASK |
		      ICE_FORCE_HW_KEY1_SETTING_MASK)) {
		printf("ICE: Fuses are blown; ICE is unusable!\n");
		return false;
	}

	return true;
}

/**
 * qcom_ice_wait_bist_status() - Wait for ICE BIST completion
 * @ice_base: ICE register base address
 *
 * Returns: 0 on success, -ETIMEDOUT on timeout
 */
static int qcom_ice_wait_bist_status(void __iomem *ice_base)
{
	u32 regval;
	int ret;

	debug("ICE: Waiting for BIST completion\n");

	ret = readl_poll_timeout(ice_base + QCOM_ICE_REG_BIST_STATUS,
				 regval, !(regval & ICE_BIST_STATUS_MASK),
				 ICE_BIST_TIMEOUT_US);
	if (ret) {
		printf("ICE: Timed out waiting for ICE self-test to complete\n");
		return ret;
	}

	debug("ICE: BIST completed successfully\n");
	return 0;
}

/**
 * qcom_ice_configure_advanced_control() - Configure ICE advanced control settings
 * @ice_base: ICE register base address
 * @enable_low_power: Enable low power mode (clock gating)
 * @enable_optimizations: Enable performance optimizations
 */
static void qcom_ice_configure_advanced_control(void __iomem *ice_base,
						bool enable_low_power,
						bool enable_optimizations)
{
	u32 regval;

	regval = readl(ice_base + QCOM_ICE_REG_ADVANCED_CONTROL);

	if (enable_low_power) {
		regval |= ICE_ADV_CTRL_LOW_POWER_MODE;
		debug("ICE: Enabling low power mode (clock gating)\n");
	}

	if (enable_optimizations) {
		regval |= ICE_ADV_CTRL_PERF_OPTIMIZE;
		debug("ICE: Enabling performance optimizations\n");
	}

	/* ICE HPG requires delay before writing */
	udelay(5);
	writel(regval, ice_base + QCOM_ICE_REG_ADVANCED_CONTROL);
	udelay(5);
	mb();
}

/**
 * qcom_ice_enable_test_bus_config() - Enable ICE test bus configuration
 * @ice_base: ICE register base address
 */
static void qcom_ice_enable_test_bus_config(void __iomem *ice_base)
{
	u32 regval;

	regval = readl(ice_base + QCOM_ICE_REG_TEST_BUS_CONTROL);
	regval &= ICE_TEST_BUS_SELECTOR_MASK;
	regval |= ICE_TEST_BUS_REG_ENABLE;
	writel(regval, ice_base + QCOM_ICE_REG_TEST_BUS_CONTROL);
	mb();
}

/**
 * qcom_ice_clear_errors() - Clear any existing error status
 * @ice_base: ICE register base address
 */
static void qcom_ice_clear_errors(void __iomem *ice_base)
{
	debug("ICE: Clearing error status registers\n");

	writel(0, ice_base + QCOM_ICE_REG_GENERAL_ERR_STTS);

	writel(0, ice_base + QCOM_ICE_REG_INVALID_CCFG_ERR_STTS);

	writel(0, ice_base + QCOM_ICE_REG_STREAM1_ERROR_SYNDROME1);
	writel(0, ice_base + QCOM_ICE_REG_STREAM1_ERROR_SYNDROME2);
	writel(0, ice_base + QCOM_ICE_REG_STREAM1_ERROR_SYNDROME3);
	writel(0, ice_base + QCOM_ICE_REG_STREAM2_ERROR_SYNDROME1);
	writel(0, ice_base + QCOM_ICE_REG_STREAM2_ERROR_SYNDROME2);
	writel(0, ice_base + QCOM_ICE_REG_STREAM2_ERROR_SYNDROME3);

	mb();
}

/**
 * qcom_ice_enable() - Enable ICE hardware
 * @ice_base: ICE register base address
 *
 * Returns: 0 on success, negative error code on failure
 */
int qcom_ice_enable(void __iomem *ice_base)
{
	int ret;

	if (!ice_base) {
		printf("ICE: Invalid ICE base address\n");
		return -EINVAL;
	}

	debug("ICE: Enabling ICE hardware\n");

	/* Clear any existing errors */
	qcom_ice_clear_errors(ice_base);

	/* Configure advanced control with low power and optimizations */
	qcom_ice_configure_advanced_control(ice_base, true, true);

	qcom_ice_enable_test_bus_config(ice_base);

	/* Wait for BIST completion */
	ret = qcom_ice_wait_bist_status(ice_base);
	if (ret) {
		printf("ICE: BIST failed: %d\n", ret);
		return ret;
	}

	debug("ICE: ICE hardware enabled successfully\n");
	return 0;
}

/**
 * qcom_ice_configure_hci() - Configure ICE HCI support
 * @cmdq_base: CMDQ register base address
 * @enable: true to enable, false to disable
 *
 * Returns: 0 on success, negative error code on failure
 */
int qcom_ice_configure_hci(void __iomem *cmdq_base, bool enable)
{
	u32 config = 0;
	u32 ice_cap = 0;

	if (!cmdq_base) {
		printf("ICE: Invalid CMDQ base address for HCI control\n");
		return -EINVAL;
	}

	debug("ICE: %s ICE HCI support\n", enable ? "Enabling" : "Disabling");

	/* Check if ICE HCI capability support is present */
	ice_cap = readl(cmdq_base + ICE_CQ_CAPABILITIES);
	if (!(ice_cap & ICE_HCI_SUPPORT)) {
		debug("ICE: HCI capability not supported (cap: 0x%x)\n", ice_cap);
		return -ENOTSUPP;
	}

	config = readl(cmdq_base + ICE_CQ_CONFIG);

	if (enable)
		config |= CRYPTO_GENERAL_ENABLE;
	else
		config &= ~CRYPTO_GENERAL_ENABLE;

	writel(config, cmdq_base + ICE_CQ_CONFIG);
	debug("ICE: HCI capability %s (cap: 0x%x)\n",
	      enable ? "enabled" : "disabled", ice_cap);

	mb();
	return 0;
}

/**
 * qcom_ice_config_crypto() - Configure ICE crypto parameters
 * @cmdq_base: CMDQ register base address
 * @dun: Data Unit Number (LBA for storage)
 * @bypass: true to bypass crypto, false to enable
 * @key_index: Key index to use
 *
 * Returns: 0 on success, negative error code on failure
 */
int qcom_ice_config_crypto(void __iomem *cmdq_base, u64 dun, bool bypass,
			   u8 key_index)
{
	u32 crypto_params = 0;
	u32 ice_cap = 0;

	if (!cmdq_base) {
		printf("ICE: Invalid CMDQ base address for crypto config\n");
		return -EINVAL;
	}

	if (key_index > MASK_SDHCI_MSM_ICE_HCI_PARAM_CCI) {
		printf("ICE: Invalid key index %d (max: %d)\n",
		       key_index, MASK_SDHCI_MSM_ICE_HCI_PARAM_CCI);
		return -EINVAL;
	}

	/* Check if ICE HCI is supported */
	ice_cap = readl(cmdq_base + ICE_CQ_CAPABILITIES);
	if (!(ice_cap & ICE_HCI_SUPPORT)) {
		debug("ICE: HCI not supported, skipping crypto config\n");
		return -ENOTSUPP;
	}

	if (dun > 0xFFFFFFFF) {
		printf("ICE: DUN value 0x%llx exceeds 32-bit range\n", dun);
		return -ERANGE;
	}

	crypto_params |=
		((!bypass) & MASK_SDHCI_MSM_ICE_HCI_PARAM_CE)
		 << OFFSET_SDHCI_MSM_ICE_HCI_PARAM_CE;

	crypto_params |= (key_index &
			 MASK_SDHCI_MSM_ICE_HCI_PARAM_CCI)
			 << OFFSET_SDHCI_MSM_ICE_HCI_PARAM_CCI;

	writel(crypto_params, cmdq_base + ICE_NONCQ_CRYPTO_PARAMS);
	writel((u32)dun, cmdq_base + ICE_NONCQ_CRYPTO_DUN);

	mb();

	debug("ICE: Crypto config - DUN: 0x%llx, bypass: %d, key_idx: %d\n",
	      dun, bypass, key_index);

	return 0;
}

/**
 * qcom_ice_reset() - Perform ICE software reset
 * @ice_base: ICE register base address
 *
 * Returns: 0 on success, negative error code on failure
 */
int qcom_ice_reset(void __iomem *ice_base)
{
	u32 regval;
	int ret;

	if (!ice_base) {
		printf("ICE: Invalid ICE base address for reset\n");
		return -EINVAL;
	}

	debug("ICE: Performing software reset\n");

	regval = readl(ice_base + QCOM_ICE_REG_ADVANCED_CONTROL);
	regval |= ICE_SW_RST_EN;
	writel(regval, ice_base + QCOM_ICE_REG_ADVANCED_CONTROL);
	mb();

	/* Wait for reset to complete */
	ret = readl_poll_timeout(ice_base + QCOM_ICE_REG_ADVANCED_CONTROL,
				 regval, !(regval & ICE_SW_RST_EN),
				 ICE_RESET_TIMEOUT_US);
	if (ret) {
		printf("ICE: Software reset timeout\n");
		return ret;
	}

	debug("ICE: Software reset completed\n");
	return 0;
}

/**
 * storage_crypto_config() - Generic storage crypto configuration function
 * @dun: Data Unit Number (LBA for storage)
 * @enable: true to enable crypto, false to disable
 *
 * This function provides a generic interface for storage crypto configuration.
 * It combines crypto parameter setup and crypto enable/disable functionality.
 *
 */
void storage_crypto_config(u64 dun, bool enable)
{
	uint32_t reg_val;
	u8 key_index = 0;  /* Use key index 0 for crashdump */
	bool bypass = false;
	int ret;
	u32 ice_cap;

	if (!g_cmdq_base) {
		printf("ICE: CMDQ base not available for crypto config\n");
		return;
	}

	/* Validate DUN is within 32-bit range for hardware */
	if (dun > 0xFFFFFFFF) {
		printf("ICE: Invalid DUN value 0x%llx (exceeds 32-bit range)\n", dun);
		return;
	}

	/* Check HCI support once */
	ice_cap = readl(g_cmdq_base + ICE_CQ_CAPABILITIES);
	if (!(ice_cap & ICE_HCI_SUPPORT)) {
		debug("ICE: HCI not supported, skipping crypto config\n");
		return;
	}

	if (enable) {
		ret = qcom_ice_config_crypto(g_cmdq_base, dun, bypass, key_index);
		if (ret) {
			printf("ICE: Failed to configure crypto: %d\n", ret);
			return;
		}
	}

	reg_val = readl(g_cmdq_base + ICE_CQ_CONFIG);

	if (enable) {
		reg_val |= CRYPTO_GENERAL_ENABLE;
		debug("ICE: Enabling CRYPTO_GENERAL_ENABLE bit\n");
	} else {
		reg_val &= ~CRYPTO_GENERAL_ENABLE;
		debug("ICE: Disabling CRYPTO_GENERAL_ENABLE bit\n");
	}

	writel(reg_val, g_cmdq_base + ICE_CQ_CONFIG);
	mb();

	debug("ICE: Generic crypto config - DUN: 0x%llx, enable: %d, reg: 0x%x\n",
	      dun, enable, reg_val);
}

/**
 * qcom_ice_init() - Initialize ICE hardware
 * @ice_base: ICE register base address
 * @cmdq_base: CMDQ register base address
 *
 * Returns: 0 on success, negative error code on failure
 */
int qcom_ice_init(void __iomem *ice_base, void __iomem *cmdq_base)
{
	int ret;

	if (!ice_base) {
		printf("ICE: Invalid ICE base address\n");
		return -EINVAL;
	}

	debug("ICE: Starting initialization\n");

	/* Store CMDQ base address in global variable for later use */
	g_cmdq_base = cmdq_base;
	if (cmdq_base) {
		debug("ICE: Stored CMDQ base address: %p\n", cmdq_base);
	}

	/* Check ICE support and version */
	if (!qcom_ice_check_supported(ice_base)) {
		printf("ICE: Hardware not supported or fuses blown\n");
		return -ENODEV;
	}

	/* Enable ICE hardware */
	ret = qcom_ice_enable(ice_base);
	if (ret) {
		printf("ICE: Failed to enable ICE hardware: %d\n", ret);
		return ret;
	}

	/* Configure ICE HCI if CMDQ is available */
	if (cmdq_base) {
		ret = qcom_ice_configure_hci(cmdq_base, true);
		if (ret) {
			printf("ICE: Failed to configure HCI: %d\n", ret);
			return ret;
		}
	}

	printf("ICE: Initialization completed successfully\n");
	return 0;
}

/**
 * qcom_ice_enable_clock() - Enable ICE clock using device tree method
 * @dev: Device pointer
 * @ice_clk: Pointer to store the enabled clock
 *
 * This function enables the ICE clock using the same method as the MSM SDHCI driver
 * by reading clock information directly from the device tree.
 *
 * Returns: true if clock enabled successfully, false otherwise
 */
static bool qcom_ice_enable_clock(struct udevice *dev, struct clk *ice_clk)
{
	int node = dev_of_offset(dev);
	int num_clocks = 3;
	int expected_array_size = num_clocks * 2;
	uint clkd[expected_array_size]; /* clk_id and clk_no for 3 clocks */
	int clk_offset;
	struct udevice *clk_dev;
	int ret;
	int ice_clk_index = -1;
	int i;
	int actual_count;

	actual_count = fdtdec_get_int_array_count(gd->fdt_blob, node, "clock", clkd, expected_array_size);
	if (actual_count < 0) {
		printf("ICE: Failed to read clock array from device tree: %d\n", actual_count);
		return false;
	}
	if (actual_count > expected_array_size) {
		printf("ICE: Invalid clock array size: %d (expected max: %d)\n",
			actual_count, expected_array_size);
		return false;
	}

	/* Find ICE clock index dynamically by matching clock ID */
	for (i = 0; i < num_clocks; i++) {
		int clk_id_index = (i * 2) + 1; /* Each clock has 2 entries: phandle + clock_id */
		if (clk_id_index >= expected_array_size) {
			printf("ICE: Clock index out of bounds\n");
			return false;
		}
		if (clkd[clk_id_index] == GCC_SDCC1_ICE_CORE_CLK) {
			ice_clk_index = clk_id_index;
			printf("ICE: Found ICE clock (ID: %d) at index %d\n",
				   GCC_SDCC1_ICE_CORE_CLK, i);
			break;
		}
	}

	if (ice_clk_index < 0) {
		printf("ICE: ICE clock (ID: %d) not found in clock array\n",
			   GCC_SDCC1_ICE_CORE_CLK);
		return false;
	}

	/* Get clock device */
	clk_offset = fdt_node_offset_by_phandle(gd->fdt_blob, clkd[0]);
	if (clk_offset < 0) {
		printf("ICE: Failed to find clock device: %d\n", clk_offset);
		return false;
	}

	ret = uclass_get_device_by_of_offset(UCLASS_CLK, clk_offset, &clk_dev);
	if (ret) {
		printf("ICE: Failed to get clock device: %d\n", ret);
		return false;
	}

	/* Enable ICE clock using dynamically found index */
	ice_clk->id = clkd[ice_clk_index];
	ret = clk_request(clk_dev, ice_clk);
	if (ret < 0) {
		printf("ICE: Failed to request ICE clock: %d\n", ret);
		return false;
	}

	/* Set ICE clock rate to 300 MHz */
	ret = clk_set_rate(ice_clk, 300000000);
	if (ret < 0) {
		printf("ICE: Failed to set ICE clock rate: %d\n", ret);
		clk_free(ice_clk);
		return false;
	}

	ret = clk_enable(ice_clk);
	if (ret < 0) {
		printf("ICE: Failed to enable ICE clock: %d\n", ret);
		clk_free(ice_clk);
		return false;
	}

	debug("ICE: ICE clock (ID: %d) enabled at 300 MHz\n",
	      clkd[ice_clk_index]);
	return true;
}

static const struct udevice_id sdhci_msm_v5_ids[] = {
	{ .compatible = "qti,sdhci-msm-v5" },
	{ }
};

/**
 * qcom_ice_init_crashdump() - Initialize ICE hardware for crashdump with device tree addresses
 *
 * This function reads the ICE and CMDQ base addresses from the device tree,
 * enables the ICE clock, and initializes the ICE hardware for crashdump operations.
 *
 * Returns: 0 on success, negative error code on failure
 */
int qcom_ice_init_crashdump(void)
{
	struct udevice *dev;
	void __iomem *ice_base = NULL;
	void __iomem *cmdq_base = NULL;
	struct clk ice_clk;
	int ret;
	bool clk_enabled = false;

	ofnode node = ofnode_null();
	int i;

	memset(&ice_clk, 0, sizeof(ice_clk));

	for (i = 0; sdhci_msm_v5_ids[i].compatible; i++) {
		node = ofnode_by_compatible(ofnode_null(), sdhci_msm_v5_ids[i].compatible);
		if (ofnode_valid(node)) {
			printf("ICE: Found compatible node: %s\n", sdhci_msm_v5_ids[i].compatible);
			break;
		}
	}

	if (!ofnode_valid(node)) {
		printf("ICE: No compatible MMC device found\n");
		return -ENODEV;
	}

	ret = uclass_get_device_by_ofnode(UCLASS_MMC, node, &dev);
	if (ret) {
		printf("ICE: Failed to get device by ofnode: %d\n", ret);
		return ret;
	}

	debug("ICE: Found MMC device: %s\n", dev->name);

	/* Enable ICE clock using dedicated function */
	clk_enabled = qcom_ice_enable_clock(dev, &ice_clk);
	if (!clk_enabled) {
		printf("ICE: Failed to enable ICE clock, aborting initialization\n");
		return -EIO;
	}

	ice_base = (void *)dev_remap_addr_name(dev, "ice_mem");
	if (!ice_base || (ulong)ice_base == FDT_ADDR_T_NONE) {
		printf("ICE: Failed to get ICE base from device tree\n");
		ice_base = NULL;
		ret = -EINVAL;
		goto cleanup_clock;
	}

	cmdq_base = (void *)dev_remap_addr_name(dev, "cmdq_mem");
	if (!cmdq_base || (ulong)cmdq_base == FDT_ADDR_T_NONE) {
		printf("ICE: Failed to get CMDQ base from device tree\n");
		cmdq_base = NULL;
		ret = -EINVAL;
		goto cleanup_clock;
	}

	debug("ICE: Device tree addresses - ICE: %p, CMDQ: %p\n",
			ice_base, cmdq_base);

	/* Initialize ICE hardware */
	ret = qcom_ice_init(ice_base, cmdq_base);
	if (ret) {
		printf("ICE: Failed to initialize ICE hardware: %d\n", ret);
		goto cleanup_clock;
	}

	return 0;

cleanup_clock:
	if (clk_enabled) {
		clk_disable(&ice_clk);
		clk_free(&ice_clk);
	}
	return ret;
}
