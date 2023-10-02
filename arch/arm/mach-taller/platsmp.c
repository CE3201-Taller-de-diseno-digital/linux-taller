// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2010-2011 Calxeda, Inc.
 * Copyright 2012 Pavel Machek <pavel@denx.de>
 * Based on platsmp.c, Copyright (C) 2002 ARM Ltd.
 * Copyright (C) 2012 Altera Corporation
 * Copyright (C) 2023 Alejandro Soto
 */
#include <linux/init.h>
#include <linux/device.h>
#include <linux/smp.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <asm/smp_plat.h>

extern const char secondary_trampoline[], secondary_trampoline_end[];

static void __iomem *smp_ctrl;

static void __init taller_smp_prepare_cpus(unsigned int max_cpus)
{
	int i;
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "itcr,smp-ctrl");
	if (!np) {
		pr_err("No SMP controller found in dt\n");
		return;
	}

	smp_ctrl = of_iomap(np, 0);
	of_node_put(np);

	if (!smp_ctrl) {
		pr_err("Failed to iomap SMP controller\n");
		return;
	}

	for (i = 0; i < 4; i++)
		set_cpu_possible(i, true);
}

static int taller_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	const unsigned long trampoline_size = secondary_trampoline_end - secondary_trampoline;
	if (!smp_ctrl)
		return -EINVAL;

	memcpy(phys_to_virt(0), secondary_trampoline, trampoline_size);
	smp_wmb();

	writel(1 << cpu, smp_ctrl);
	return 0;
}

static const struct smp_operations taller_smp_ops __initconst = {
	.smp_prepare_cpus	= taller_smp_prepare_cpus,
	.smp_boot_secondary	= taller_boot_secondary,
};
CPU_METHOD_OF_DECLARE(taller_smp, "itcr,taller-smp", &taller_smp_ops);
