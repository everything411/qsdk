/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/of.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <nss_dp_arch.h>
#include "nss_dp_hal.h"
#include "edma_debug.h"
#include "edma.h"

#ifdef CONFIG_IO_COHERENCY
#include "edma_regs.h"
#include <linux/tmelcom_ipc.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#endif

/*
 * nss_dp_hal_nsm_sawf_sc_stats_read()
 *	Send nsm stats for the given service-class.
 */
bool nss_dp_hal_nsm_sawf_sc_stats_read(struct nss_dp_hal_nsm_sawf_sc_stats *nsm_stats, uint8_t service_class)
{
	return edma_nsm_sawf_sc_stats_read(nsm_stats, service_class);
}

/*
 * nss_dp_hal_get_data_plane_ops()
 *	Return the data plane ops for registered data plane.
 */
struct nss_dp_data_plane_ops *nss_dp_hal_get_data_plane_ops(void)
{
	return &nss_dp_edma_ops;
}

/*
 * nss_dp_hal_deinit_soc_priv_flags()
 *	API to de initialize DP DEV flags field
 */
void nss_dp_hal_deinit_soc_priv_flags(struct nss_dp_dev *dp_priv)
{
	clear_bit(__NSS_DP_NO_LIST, &dp_priv->flags);
}

/*
 * nss_dp_hal_init_soc_priv_flags()
 *	API to initialize DP DEV flags field
 */
void nss_dp_hal_init_soc_priv_flags(struct nss_dp_dev *dp_priv)
{
	set_bit(__NSS_DP_NO_LIST, &dp_priv->flags);
}

#ifdef CONFIG_IO_COHERENCY

/*
 * nss_noc_reg_write - Write the value into NSS NOC registers
 * @reg_base: base address for the NSS NOC registers
 * @regs_off: Pointer to the register offset and data to be written into
 * @count: Number of registers
 * @secure_write: Indicates secure IO write/Regular IO write
 *
 * This function returns 0 on success, and negative error code on failure
 */
static inline int nss_noc_reg_write(uint32_t reg_base, uint32_t *regs_off, int count, bool secure_write)
{
	int i;

	for (i = 0; i < count; i++, regs_off += 2) {
		uint32_t reg_addr = reg_base + regs_off[0];
		uint32_t reg_val = regs_off[1];

		if (secure_write) {
			struct tmel_secure_io nss_noc = {0};
			int error;

			/*
			 * Write the value into the registers
			 * using secure IO.
			 */
			nss_noc.reg_addr = reg_addr;
			nss_noc.reg_val = reg_val;
			error = tmelcom_secure_io_write(&nss_noc, sizeof(struct tmel_secure_io));
			if (error) {
				pr_err("Failed to configure NSS NOC reg = 0x%x, val=0x%x\n", reg_addr, reg_val);
				return error;
			}
		} else {
			void __iomem *map_addr;

			/*
			 * Write the value into the registers
			 * using regular IO mapping.
			 */
			map_addr = ioremap(reg_addr, sizeof(uint32_t));
			if (!map_addr) {
				pr_err("Failed to configure NSS NOC reg = 0x%x, val=0x%x\n", reg_addr, reg_val);
				return -EINVAL;
			}

			writel(reg_val, map_addr);
			iounmap(map_addr);
		}

		pr_debug("Configuring nss_noc for addr: (0x%x), val: (0x%x)\n", reg_addr, reg_val);
	}

	return 0;
}

/*
 * nss_noc_reg_update - Update NSS NOC register settings
 * @pdev: Pointer to the platform device structure
 *
 * This function updates the NSS NoC Register addresses by writing
 * the values to the respective registers.
 *
 * Return: 0 on success, negative error code on failure.
 */
static inline int nss_noc_reg_update(struct platform_device *pdev)
{
	int ret, count;
	struct device_node *np = (&pdev->dev)->of_node;
	struct device_node *child = NULL;
	bool secure_write = false;
	uint32_t *regs_off = NULL;
	uint32_t reg_base;

	edma_set_noc_stage(EDMA_NOC_STAGE_DT_PARSE_START);

	for_each_available_child_of_node(np, child) {
		/*
		 * Only read the registers if the corresponding (nss-noc)
		 * property is defined in the DTSI.
		 * Else, bypass the IO write operations.
		 */
		if (of_property_match_string(child, "prop-name", "nss_noc") < 0)
			continue;

		edma_set_noc_stage(EDMA_NOC_STAGE_DT_PARSE_DONE);

		secure_write = of_property_read_bool(child, "secure-write");

		/*
		 * Read the Register base address from the DTSI.
		 */
		ret = of_property_read_u32(child, "reg-base", &reg_base);
		if (ret) {
			edma_err("%px: Failed to read the Register base address\n", child);
			return ret;
		}
		edma_set_noc_stage(EDMA_NOC_STAGE_REG_BASE_READ);

		/*
		 * Read the number of register elements.
		 */
		count = of_property_count_u32_elems(child, "reg-offset");
		if ((count == 0) || (count % 2)) {
			edma_err("%px: Invalid entries obtained from the DTSI\n", child);
			return -EINVAL;
		}
		edma_set_noc_stage(EDMA_NOC_STAGE_REG_COUNT_READ);

		/*
		 * Allocate memory for reading NOC register
		 * values from DTSI.
		 */
		regs_off = vmalloc(sizeof(u32) * count);
		if (!regs_off) {
			edma_err("%px: Failed to allocate memory for reading NOC regs\n", child);
			return -EINVAL;
		}
		edma_set_noc_stage(EDMA_NOC_STAGE_MEM_ALLOC_DONE);

		/*
		 * Read the register offsets and their values
		 * from the DTSI and write into address.
		 */
		ret = of_property_read_u32_array(child, "reg-offset", regs_off, count);
		if (ret) {
			edma_err("%px: Error in fetching the offset address and value for Register\n", child);
			goto fail;
		}
		edma_set_noc_stage(EDMA_NOC_STAGE_REG_OFFSET_READ);

		edma_set_noc_stage(EDMA_NOC_STAGE_REG_WRITE_START);
		ret = nss_noc_reg_write(reg_base, regs_off, count/2, secure_write);
		if (ret)
			goto fail;
		edma_set_noc_stage(EDMA_NOC_STAGE_REG_WRITE_DONE);

		vfree(regs_off);
		edma_set_noc_stage(EDMA_NOC_STAGE_MEM_FREE_DONE);
		return 0;
fail:
		vfree(regs_off);
		return ret;
	}

	return 0;
}
#endif

/*
 * nss_dp_hal_clock_set_and_enable()
 *	API to set and enable the EDMA common clocks
 */
int32_t nss_dp_hal_clock_set_and_enable(struct device *dev, const char *id, unsigned long rate)
{
	struct clk *clk = NULL;
	int err;

	clk = devm_clk_get(dev, id);
	if (IS_ERR(clk)) {
		return -1;
	}

	if (rate) {
		err = clk_set_rate(clk, rate);
		if (err) {
			return -1;
		}
	}

	err = clk_prepare_enable(clk);
	if (err) {
		return -1;
	}

	return 0;
}

#ifdef CONFIG_IO_COHERENCY
/*
 * nss_dp_hal_configure_llc()
 *	Writes into the EDMA Descriptor rings cache registers.
 */
static int nss_dp_hal_configure_llc(struct edma_gbl_ctx *egc, struct device_node *np)
{
	int ring_idx, count, ret;

	/*
	 * Read the number of elements.
	 */
	count = of_property_count_u32_elems(np, "cache_val");
	if (!count) {
		pr_err("%px: Invalid entries obtained from the DTSI\n", np);
		return -EINVAL;
	}

	/*
	 * Allocate memory for reading cache
	 * register data from DTSI.
	 */
	egc->cache_data = vmalloc(sizeof(u32) * count);
	if (!egc->cache_data) {
		pr_err("%px: Failed to allocate memory for reading cache register data\n", np);
		return -EINVAL;
	}

	/*
	 * Read the cache register data
	 * from the DTSI.
	 */
	ret = of_property_read_u32_array(np, "cache_val", egc->cache_data, count);
	if (ret) {
		pr_err("%px: Error in fetching the data for cache Registers\n", np);
		goto fail;
	}

	/*
	 * Write the data into cache registers
	 */
	for (ring_idx = 0; ring_idx < EDMA_MAX_RXDESC_RINGS; ring_idx++) {
		edma_reg_write(EDMA_REG_RXDESC_CACHE(ring_idx), egc->cache_data[0]);
	}

	for (ring_idx = 0; ring_idx < EDMA_MAX_TXCMPL_RINGS; ring_idx++) {
		edma_reg_write(EDMA_REG_TXCMPL_CACHE(ring_idx), egc->cache_data[1]);
	}

	edma_reg_write(EDMA_REG_CACHEINDEX_LUT, egc->cache_data[2]);
	edma_reg_write(EDMA_REG_AXCACHE_OVERRIDE, egc->cache_data[3]);
	edma_reg_write(EDMA_REG_AXIW_CTRL, egc->cache_data[4]);
	vfree(egc->cache_data);
	return 0;

fail:
	vfree(egc->cache_data);
	return ret;
}
#endif

/*
 * nss_dp_hal_cache_info_setup()
 *	Setup the Descriptor rings cache data
 *	from the DTSI.
 *
 * Returns 0: success and a negative error code on failure.
 */
int nss_dp_hal_cache_info_setup(void *ctx)
{
	struct edma_gbl_ctx *egc = (struct edma_gbl_ctx *)ctx;
	egc->cache_data = NULL;

#ifdef CONFIG_IO_COHERENCY
	struct platform_device *pdev = egc->pdev;
	struct device_node *np = (&pdev->dev)->of_node;
	struct device_node *child = NULL;

	for_each_available_child_of_node(np, child) {
		/*
		 * Read the EDMA Descriptor rings cache register data
		 * defined in the DTSI.
		 */
		if (!of_property_match_string(child, "prop-name", "llcc_cache"))
			return nss_dp_hal_configure_llc(egc, child);
	}
#endif
	return 0;
}

/*
 * nss_dp_hal_configure_clocks()
 *	configure the EDMA clock's.
 */
int32_t nss_dp_hal_configure_clocks(void *ctx)
{
	struct platform_device *pdev = (struct platform_device *)ctx;
	int32_t err;

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_CSR_CLK, NSS_DP_EDMA_CSR_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_CSR_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_CSR);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_NSSNOC_CSR_CLK, NSS_DP_EDMA_NSSNOC_CSR_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_NSSNOC_CSR_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_NSSNOC_CSR);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_CC_CE_APB_CLK, NSS_DP_EDMA_CC_CE_APB_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_CC_CE_APB_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_CC_CE_APB);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_CC_CE_AXI_CLK, NSS_DP_EDMA_CC_CE_AXI_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_CC_CE_AXI_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_CC_CE_AXI);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_CC_NSSNOC_CE_APB_CLK,
					NSS_DP_EDMA_CC_NSSNOC_CE_APB_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_CC_NSSNOC_CE_APB_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_CC_NSSNOC_CE_APB);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_CC_NSSNOC_CE_AXI_CLK,
					NSS_DP_EDMA_CC_NSSNOC_CE_AXI_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_CC_NSSNOC_CE_AXI_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_CC_NSSNOC_CE_AXI);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_NSSCC_CLK, NSS_DP_EDMA_NSSCC_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_NSSCC_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_NSSCC);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_NSSCFG_CLK, NSS_DP_EDMA_NSSCFG_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_NSSCFG_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_NSSCFG);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_NSSNOC_NSSCC_CLK, NSS_DP_EDMA_NSSNOC_NSSCC_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_NSSNOC_NSSCC_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_NSSNOC_NSSCC);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_TS_CLK, NSS_DP_EDMA_TS_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_TS_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_TS);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_NSSNOC_PCNOC_1_CLK,
					NSS_DP_EDMA_NSSNOC_PCNOC_1_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_NSSNOC_PCNOC_1_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_NSSNOC_PCNOC_1);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_NSSNOC_ATB_CLK,
					NSS_DP_EDMA_NSSNOC_ATB_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_NSSNOC_ATB_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_NSSNOC_ATB);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_NSSNOC_QOSGEN_REF_CLK,
					NSS_DP_EDMA_NSSNOC_QOSGEN_REF_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_NSSNOC_QOSGEN_REF_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_NSSNOC_QOSGEN_REF);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_NSSNOC_SNOC_1_CLK,
					NSS_DP_EDMA_NSSNOC_SNOC_1_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_NSSNOC_SNOC_1_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_NSSNOC_SNOC_1);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_NSSNOC_SNOC_CLK,
					NSS_DP_EDMA_NSSNOC_SNOC_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_NSSNOC_SNOC_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_NSSNOC_SNOC);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_NSSNOC_TIMEOUT_REF_CLK,
					NSS_DP_EDMA_NSSNOC_TIMEOUT_REF_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_NSSNOC_TIMEOUT_REF_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_NSSNOC_TIMEOUT_REF);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_NSSNOC_XO_DCD_CLK,
					NSS_DP_EDMA_NSSNOC_XO_DCD_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_NSSNOC_XO_DCD_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_NSSNOC_XO_DCD);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_NSSNOC_MEMNOC_CLK,
					NSS_DP_EDMA_NSSNOC_MEMNOC_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_NSSNOC_MEMNOC_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_NSSNOC_MEMNOC);

	err = nss_dp_hal_clock_set_and_enable(&pdev->dev, NSS_DP_EDMA_NSSNOC_MEM_NOC_1_CLK,
					NSS_DP_EDMA_NSSNOC_MEM_NOC_1_CLK_FREQ);
	if (err) {
		edma_err("Error: HAL failed to configure %s clock\n", NSS_DP_EDMA_NSSNOC_MEM_NOC_1_CLK);
		return -1;
	}
	edma_set_clk_stage(EDMA_CLK_STAGE_NSSNOC_MEM_NOC_1);

#ifdef CONFIG_IO_COHERENCY
	/*
	 * TODO: Get rid of the above compile time MACRO and
	 * invoke the reg_update API based on the global flag
	 * from the DTSI.
	 */
	err = nss_noc_reg_update(pdev);
        if (err) {
		edma_err("Error: NSS NOC register configurations failed\n");
                return -1;
        }
	edma_set_clk_stage(EDMA_CLK_STAGE_NSS_NOC_REG);
#endif
	return 0;
}

/*
 * nss_dp_hal_hw_reset()
 *	Reset EDMA hardware.
 */
int32_t nss_dp_hal_hw_reset(void *ctx)
{
	struct reset_control *edma_hw_rst, *edma_cfg_rst;
	struct platform_device *pdev = (struct platform_device *)ctx;

	if (!edma_hang_recover)
		return 0;

	edma_hw_rst = devm_reset_control_get(&pdev->dev, EDMA_HW_RESET_ID);
	if (IS_ERR(edma_hw_rst)) {
		return -EINVAL;
	}

	edma_cfg_rst = devm_reset_control_get(&pdev->dev, EDMA_CFG_RESET_ID);
	if (IS_ERR(edma_hw_rst)) {
		return -EINVAL;
	}

	/*
	 * Store the obtained hardware reset handle (`edma_hw_rst`) in the global context
	 * (`edma_gbl_ctx`) for future use. This allows for centralized reset control
	 * throughout the driver.
	 *
	 * TODO: Revisit if this global storage is actually required.
	 */
	edma_gbl_ctx->hw_rst = edma_hw_rst;

	/*
	 * Store the obtained edma configuration reset handle (`edma_cfg_rst`) in the global context
	 * (`edma_gbl_ctx`) for future use. This allows for centralized configuration reset control
	 * throughout the driver.
	 */
	edma_gbl_ctx->cfg_rst = edma_cfg_rst;

	reset_control_assert(edma_hw_rst);
	udelay(100);

	reset_control_deassert(edma_hw_rst);
	udelay(100);

	/*
	 * EDMA configuration reset.
	 */
	reset_control_assert(edma_cfg_rst);
	udelay(100);

	reset_control_deassert(edma_cfg_rst);
	udelay(100);

	return 0;
}

/*
 * nss_dp_hal_init()
 *	Initialize EDMA and set gmac ops.
 */
bool nss_dp_hal_init(void)
{
	/*
	 * Bail out on not supported platform.
	 * TODO: Remove the devsoc machine compatibility check after SOD.
	 */
	if (!of_machine_is_compatible("qcom,devsoc")
		&& !of_machine_is_compatible("qcom,ipq5424")) {
		return false;
	}

	if (edma_init()) {
		return false;
	}

	nss_dp_hal_set_gmac_ops(&qcom_gmac_ops, GMAC_HAL_TYPE_QCOM);
	nss_dp_hal_set_gmac_ops(&syn_gmac_ops, GMAC_HAL_TYPE_SYN_XGMAC);

	return true;
}

/*
 * nss_dp_hal_cleanup()
 *	Cleanup EDMA and set gmac ops to NULL.
 */
void nss_dp_hal_cleanup(void)
{
	nss_dp_hal_set_gmac_ops(NULL, GMAC_HAL_TYPE_QCOM);
	nss_dp_hal_set_gmac_ops(NULL, GMAC_HAL_TYPE_SYN_XGMAC);
	edma_cleanup(false);
}

/*
 * nss_dp_ppeds_wifi_arch_mode_ops_get()
 *	API to get PPE-DS operations()
 */
struct nss_dp_ppeds_ops *nss_dp_ppeds_wifi_arch_mode_ops_get(uint32_t mode)
{
#ifdef NSS_DP_PPEDS_SUPPORT
	return &edma_ppeds_ops;
#else
	return NULL;
#endif
}
