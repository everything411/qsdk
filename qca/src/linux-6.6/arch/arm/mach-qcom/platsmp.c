// SPDX-License-Identifier: GPL-2.0-only
/*
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *  Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *  Copyright (c) 2014 The Linux Foundation. All rights reserved.
 */

#include <linux/init.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/smp.h>
#include <linux/io.h>
#include <linux/firmware/qcom/qcom_scm.h>

#include <asm/smp_plat.h>


#define VDD_SC1_ARRAY_CLAMP_GFS_CTL	0x35a0
#define SCSS_CPU1CORE_RESET		0x2d80
#define SCSS_DBG_STATUS_CORE_PWRDUP	0x2e64

#define APCS_CPU_PWR_CTL	0x04
#define PLL_CLAMP		BIT(8)
#define CORE_PWRD_UP		BIT(7)
#define COREPOR_RST		BIT(5)
#define CORE_RST		BIT(4)
#define L2DT_SLP		BIT(3)
#define CORE_MEM_CLAMP		BIT(1)
#define CLAMP			BIT(0)

#define APC_PWR_GATE_CTL	0x14
#define BHS_CNT_SHIFT		24
#define LDO_PWR_DWN_SHIFT	16
#define LDO_BYP_SHIFT		8
#define BHS_SEG_SHIFT		1
#define BHS_EN			BIT(0)

#define APCS_SAW2_VCTL		0x14
#define APCS_SAW2_2_VCTL	0x1c

/* CPU power domain register offsets */
#define CPU_HEAD_SWITCH_CTL 0x8
#define CPU_SEQ_FORCE_PWR_CTL_EN 0x1c
#define CPU_SEQ_FORCE_PWR_CTL_VAL 0x20
#define CPU_PCHANNEL_FSM_CTL 0x44

extern void secondary_startup_arm(void);

#ifdef CONFIG_HOTPLUG_CPU
static void qcom_cpu_die(unsigned int cpu)
{
	wfi();
}
#endif

static inline void a55ss_unclamp_cpu(void __iomem *reg)
{
	/* Program skew between en_few and en_rest to 40 XO clk cycles (~2us) */
	writel_relaxed(0x00000028, reg + CPU_HEAD_SWITCH_CTL);
	mb();

	/* Current sensors bypass */
	writel_relaxed(0x00000000, reg + CPU_SEQ_FORCE_PWR_CTL_EN);
	mb();

	/* Close Core logic head switch */
	writel_relaxed(0x00000642, reg + CPU_SEQ_FORCE_PWR_CTL_VAL);
	mb();
	mdelay(4);

	/* Deassert Core Mem and Logic Clamp. (Clamp coremem is asserted by default) */
	writel_relaxed(0x00000402, reg + CPU_SEQ_FORCE_PWR_CTL_VAL);
	mb();

	/* De-Assert Core memory slp_nret_n */
	writel_relaxed(0x0000040A, reg + CPU_SEQ_FORCE_PWR_CTL_VAL);
	mb();
	mdelay(8);

	/* De-Assert Core memory slp_ret_n */
	writel_relaxed(0x0000040E, reg + CPU_SEQ_FORCE_PWR_CTL_VAL);
	mb();
	mdelay(8);

	/* Assert wl_en_clk */
	writel_relaxed(0x0000050E, reg + CPU_SEQ_FORCE_PWR_CTL_VAL);
	mb();
	mdelay(2);

	/* De-assert wl_en_clk */
	writel_relaxed(0x0000040E, reg + CPU_SEQ_FORCE_PWR_CTL_VAL);
	mb();

	/* Deassert ClkOff */
	writel_relaxed(0x0000040C, reg + CPU_SEQ_FORCE_PWR_CTL_VAL);
	mb();
	mdelay(4);

	/*Assert core pchannel power up request */
	writel_relaxed(0x00000001, reg + CPU_PCHANNEL_FSM_CTL);
	mb();

	/* Deassert Core reset */
	writel_relaxed(0x0000043C, reg + CPU_SEQ_FORCE_PWR_CTL_VAL);
	mb();

	/* Deassert core pchannel power up request */
	writel_relaxed(0x00000000, reg + CPU_PCHANNEL_FSM_CTL);
	mb();

	/* Indicate OSM that core is ACTIVE */
	writel_relaxed(0x0000443C, reg + CPU_SEQ_FORCE_PWR_CTL_VAL);
	mb();

	/* Assert CPU_PWRDUP */
	writel_relaxed(0x00000428, reg + CPU_HEAD_SWITCH_CTL);
	mb();
}

static int a55ss_release_secondary(unsigned int cpu)
{
	int ret = 0;
	struct device_node *cpu_node, *acc_node;
	void __iomem *reg;
	void __iomem *el_mem_base;

	cpu_node = of_get_cpu_node(cpu, NULL);
	if (!cpu_node)
		return -ENODEV;

	acc_node = of_parse_phandle(cpu_node, "qcom,acc", 0);
	if (!acc_node) {
		ret = -ENODEV;
		goto out_acc;
	}

	reg = of_iomap(acc_node, 0);
	if (!reg) {
		ret = -ENOMEM;
		goto out_acc_reg;
	}

	el_mem_base = ioremap(0x8A700150, 0xc);
	if (IS_ERR_OR_NULL(el_mem_base)) {
		pr_err("el_mem base ioremap is failed\n");
	} else {
		if (cpu == 0x2) { /* update core2 GICR */
			writel(0xF280024, el_mem_base + 0x0);
			writel(0xF280014, el_mem_base + 0x4);
			writel(0xF290080, el_mem_base + 0x8);
		} else if (cpu == 0x3) { /* update core3 GICR */
			writel(0xF2A0024, el_mem_base + 0x0);
			writel(0xF2A0014, el_mem_base + 0x4);
			writel(0xF2B0080, el_mem_base + 0x8);
		}
	}

	iounmap(el_mem_base);

	a55ss_unclamp_cpu(reg);

	/* Secondary CPU-N is now alive */
	iounmap(reg);

out_acc_reg:
	of_node_put(acc_node);
out_acc:
	of_node_put(cpu_node);

	return ret;
}

static int scss_release_secondary(unsigned int cpu)
{
	struct device_node *node;
	void __iomem *base;

	node = of_find_compatible_node(NULL, NULL, "qcom,gcc-msm8660");
	if (!node) {
		pr_err("%s: can't find node\n", __func__);
		return -ENXIO;
	}

	base = of_iomap(node, 0);
	of_node_put(node);
	if (!base)
		return -ENOMEM;

	writel_relaxed(0, base + VDD_SC1_ARRAY_CLAMP_GFS_CTL);
	writel_relaxed(0, base + SCSS_CPU1CORE_RESET);
	writel_relaxed(3, base + SCSS_DBG_STATUS_CORE_PWRDUP);
	mb();
	iounmap(base);

	return 0;
}

static int cortex_a7_release_secondary(unsigned int cpu)
{
	int ret = 0;
	void __iomem *reg;
	struct device_node *cpu_node, *acc_node;
	u32 reg_val;

	cpu_node = of_get_cpu_node(cpu, NULL);
	if (!cpu_node)
		return -ENODEV;

	acc_node = of_parse_phandle(cpu_node, "qcom,acc", 0);
	if (!acc_node) {
		ret = -ENODEV;
		goto out_acc;
	}

	reg = of_iomap(acc_node, 0);
	if (!reg) {
		ret = -ENOMEM;
		goto out_acc_map;
	}

	/* Put the CPU into reset. */
	reg_val = CORE_RST | COREPOR_RST | CLAMP | CORE_MEM_CLAMP;
	writel(reg_val, reg + APCS_CPU_PWR_CTL);

	/* Turn on the BHS and set the BHS_CNT to 16 XO clock cycles */
	writel(BHS_EN | (0x10 << BHS_CNT_SHIFT), reg + APC_PWR_GATE_CTL);
	/* Wait for the BHS to settle */
	udelay(2);

	reg_val &= ~CORE_MEM_CLAMP;
	writel(reg_val, reg + APCS_CPU_PWR_CTL);
	reg_val |= L2DT_SLP;
	writel(reg_val, reg + APCS_CPU_PWR_CTL);
	udelay(2);

	reg_val = (reg_val | BIT(17)) & ~CLAMP;
	writel(reg_val, reg + APCS_CPU_PWR_CTL);
	udelay(2);

	/* Release CPU out of reset and bring it to life. */
	reg_val &= ~(CORE_RST | COREPOR_RST);
	writel(reg_val, reg + APCS_CPU_PWR_CTL);
	reg_val |= CORE_PWRD_UP;
	writel(reg_val, reg + APCS_CPU_PWR_CTL);

	iounmap(reg);
out_acc_map:
	of_node_put(acc_node);
out_acc:
	of_node_put(cpu_node);
	return ret;
}

static int kpssv1_release_secondary(unsigned int cpu)
{
	int ret = 0;
	void __iomem *reg, *saw_reg;
	struct device_node *cpu_node, *acc_node, *saw_node;
	u32 val;

	cpu_node = of_get_cpu_node(cpu, NULL);
	if (!cpu_node)
		return -ENODEV;

	acc_node = of_parse_phandle(cpu_node, "qcom,acc", 0);
	if (!acc_node) {
		ret = -ENODEV;
		goto out_acc;
	}

	saw_node = of_parse_phandle(cpu_node, "qcom,saw", 0);
	if (!saw_node) {
		ret = -ENODEV;
		goto out_saw;
	}

	reg = of_iomap(acc_node, 0);
	if (!reg) {
		ret = -ENOMEM;
		goto out_acc_map;
	}

	saw_reg = of_iomap(saw_node, 0);
	if (!saw_reg) {
		ret = -ENOMEM;
		goto out_saw_map;
	}

	/* Turn on CPU rail */
	writel_relaxed(0xA4, saw_reg + APCS_SAW2_VCTL);
	mb();
	udelay(512);

	/* Krait bring-up sequence */
	val = PLL_CLAMP | L2DT_SLP | CLAMP;
	writel_relaxed(val, reg + APCS_CPU_PWR_CTL);
	val &= ~L2DT_SLP;
	writel_relaxed(val, reg + APCS_CPU_PWR_CTL);
	mb();
	ndelay(300);

	val |= COREPOR_RST;
	writel_relaxed(val, reg + APCS_CPU_PWR_CTL);
	mb();
	udelay(2);

	val &= ~CLAMP;
	writel_relaxed(val, reg + APCS_CPU_PWR_CTL);
	mb();
	udelay(2);

	val &= ~COREPOR_RST;
	writel_relaxed(val, reg + APCS_CPU_PWR_CTL);
	mb();
	udelay(100);

	val |= CORE_PWRD_UP;
	writel_relaxed(val, reg + APCS_CPU_PWR_CTL);
	mb();

	iounmap(saw_reg);
out_saw_map:
	iounmap(reg);
out_acc_map:
	of_node_put(saw_node);
out_saw:
	of_node_put(acc_node);
out_acc:
	of_node_put(cpu_node);
	return ret;
}

static int kpssv2_release_secondary(unsigned int cpu)
{
	void __iomem *reg;
	struct device_node *cpu_node, *l2_node, *acc_node, *saw_node;
	void __iomem *l2_saw_base;
	unsigned reg_val;
	int ret;

	cpu_node = of_get_cpu_node(cpu, NULL);
	if (!cpu_node)
		return -ENODEV;

	acc_node = of_parse_phandle(cpu_node, "qcom,acc", 0);
	if (!acc_node) {
		ret = -ENODEV;
		goto out_acc;
	}

	l2_node = of_parse_phandle(cpu_node, "next-level-cache", 0);
	if (!l2_node) {
		ret = -ENODEV;
		goto out_l2;
	}

	saw_node = of_parse_phandle(l2_node, "qcom,saw", 0);
	if (!saw_node) {
		ret = -ENODEV;
		goto out_saw;
	}

	reg = of_iomap(acc_node, 0);
	if (!reg) {
		ret = -ENOMEM;
		goto out_map;
	}

	l2_saw_base = of_iomap(saw_node, 0);
	if (!l2_saw_base) {
		ret = -ENOMEM;
		goto out_saw_map;
	}

	/* Turn on the BHS, turn off LDO Bypass and power down LDO */
	reg_val = (64 << BHS_CNT_SHIFT) | (0x3f << LDO_PWR_DWN_SHIFT) | BHS_EN;
	writel_relaxed(reg_val, reg + APC_PWR_GATE_CTL);
	mb();
	/* wait for the BHS to settle */
	udelay(1);

	/* Turn on BHS segments */
	reg_val |= 0x3f << BHS_SEG_SHIFT;
	writel_relaxed(reg_val, reg + APC_PWR_GATE_CTL);
	mb();
	 /* wait for the BHS to settle */
	udelay(1);

	/* Finally turn on the bypass so that BHS supplies power */
	reg_val |= 0x3f << LDO_BYP_SHIFT;
	writel_relaxed(reg_val, reg + APC_PWR_GATE_CTL);

	/* enable max phases */
	writel_relaxed(0x10003, l2_saw_base + APCS_SAW2_2_VCTL);
	mb();
	udelay(50);

	reg_val = COREPOR_RST | CLAMP;
	writel_relaxed(reg_val, reg + APCS_CPU_PWR_CTL);
	mb();
	udelay(2);

	reg_val &= ~CLAMP;
	writel_relaxed(reg_val, reg + APCS_CPU_PWR_CTL);
	mb();
	udelay(2);

	reg_val &= ~COREPOR_RST;
	writel_relaxed(reg_val, reg + APCS_CPU_PWR_CTL);
	mb();

	reg_val |= CORE_PWRD_UP;
	writel_relaxed(reg_val, reg + APCS_CPU_PWR_CTL);
	mb();

	ret = 0;

	iounmap(l2_saw_base);
out_saw_map:
	iounmap(reg);
out_map:
	of_node_put(saw_node);
out_saw:
	of_node_put(l2_node);
out_l2:
	of_node_put(acc_node);
out_acc:
	of_node_put(cpu_node);

	return ret;
}

static DEFINE_PER_CPU(int, cold_boot_done);

static int qcom_boot_secondary(unsigned int cpu, int (*func)(unsigned int))
{
	int ret = 0;

	if (!per_cpu(cold_boot_done, cpu)) {
		ret = func(cpu);
		if (!ret)
			per_cpu(cold_boot_done, cpu) = true;
	}

	/*
	 * Send the secondary CPU a soft interrupt, thereby causing
	 * the boot monitor to read the system wide flags register,
	 * and branch to the address found there.
	 */
	arch_send_wakeup_ipi_mask(cpumask_of(cpu));

	return ret;
}

static int a55ss_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	return qcom_boot_secondary(cpu, a55ss_release_secondary);
}

static int msm8660_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	return qcom_boot_secondary(cpu, scss_release_secondary);
}

static int cortex_a7_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	return qcom_boot_secondary(cpu, cortex_a7_release_secondary);
}

static int kpssv1_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	return qcom_boot_secondary(cpu, kpssv1_release_secondary);
}

static int kpssv2_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	return qcom_boot_secondary(cpu, kpssv2_release_secondary);
}

static void __init qcom_smp_prepare_cpus(unsigned int max_cpus)
{
	int cpu;

	if (qcom_scm_set_cold_boot_addr(secondary_startup_arm)) {
		for_each_present_cpu(cpu) {
			if (cpu == smp_processor_id())
				continue;
			set_cpu_present(cpu, false);
		}
		pr_warn("Failed to set CPU boot address, disabling SMP\n");
	}
}

static const struct smp_operations smp_a55ss_ops __initconst = {
	.smp_prepare_cpus	= qcom_smp_prepare_cpus,
	.smp_boot_secondary	= a55ss_boot_secondary,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_die		= qcom_cpu_die,
#endif
};

CPU_METHOD_OF_DECLARE(qcom_smp_a55ss, "qcom,arm-cortex-acc", &smp_a55ss_ops);

static const struct smp_operations smp_msm8660_ops __initconst = {
	.smp_prepare_cpus	= qcom_smp_prepare_cpus,
	.smp_boot_secondary	= msm8660_boot_secondary,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_die		= qcom_cpu_die,
#endif
};
CPU_METHOD_OF_DECLARE(qcom_smp, "qcom,gcc-msm8660", &smp_msm8660_ops);

static const struct smp_operations qcom_smp_cortex_a7_ops __initconst = {
	.smp_prepare_cpus	= qcom_smp_prepare_cpus,
	.smp_boot_secondary	= cortex_a7_boot_secondary,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_die		= qcom_cpu_die,
#endif
};
CPU_METHOD_OF_DECLARE(qcom_smp_msm8226, "qcom,msm8226-smp", &qcom_smp_cortex_a7_ops);
CPU_METHOD_OF_DECLARE(qcom_smp_msm8909, "qcom,msm8909-smp", &qcom_smp_cortex_a7_ops);
CPU_METHOD_OF_DECLARE(qcom_smp_msm8916, "qcom,msm8916-smp", &qcom_smp_cortex_a7_ops);

static const struct smp_operations qcom_smp_kpssv1_ops __initconst = {
	.smp_prepare_cpus	= qcom_smp_prepare_cpus,
	.smp_boot_secondary	= kpssv1_boot_secondary,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_die		= qcom_cpu_die,
#endif
};
CPU_METHOD_OF_DECLARE(qcom_smp_kpssv1, "qcom,kpss-acc-v1", &qcom_smp_kpssv1_ops);

static const struct smp_operations qcom_smp_kpssv2_ops __initconst = {
	.smp_prepare_cpus	= qcom_smp_prepare_cpus,
	.smp_boot_secondary	= kpssv2_boot_secondary,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_die		= qcom_cpu_die,
#endif
};
CPU_METHOD_OF_DECLARE(qcom_smp_kpssv2, "qcom,kpss-acc-v2", &qcom_smp_kpssv2_ops);
