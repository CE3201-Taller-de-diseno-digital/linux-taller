// SPDX-License-Identifier: GPL-2.0-only
/*
 * drivers/irq/irq-taller.c
 *
 * Copyright (C) 2022 Alejandro Soto
 */
#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/irq.h>
#include <linux/irqchip.h>
#include <linux/irqdomain.h>

#include <asm/exception.h>

#define TALLER_INTC_STATUS	0x00
#define TALLER_INTC_MASK	0x04

static struct irq_domain *taller_irq_domain;

static void __exception_irq_entry taller_handle_irq(struct pt_regs *regs)
{
	irq_hw_number_t hwirq;
	unsigned long status;
	struct irq_chip_generic *gc;

	gc = irq_get_domain_generic_chip(taller_irq_domain, 0);
	status = readl(gc->reg_base + TALLER_INTC_STATUS);

	while (status) {
		hwirq = __ffs(status);
		generic_handle_domain_irq(taller_irq_domain, hwirq);
		status &= ~(1 << hwirq);
	}
}

static int __init taller_intc_init(struct device_node *np,
			       struct device_node *parent)
{
	int ret;
	void __iomem *base;
	struct irq_chip_generic *gc;
	const unsigned int clr = IRQ_NOREQUEST | IRQ_NOPROBE | IRQ_NOAUTOEN;

	base = of_iomap(np, 0);
	if (IS_ERR_OR_NULL(base)) {
		pr_err("Failed to iomap interrupt controller\n");
		return base ? PTR_ERR(base) : -ENOMEM;
	}

	taller_irq_domain =
		irq_domain_add_linear(np, 32, &irq_generic_chip_ops, NULL);

	if (!taller_irq_domain) {
		pr_crit("Failed to allocate irq domain\n");
		ret = -ENOMEM;
		goto err_unmap;
	}

	ret = irq_alloc_domain_generic_chips(taller_irq_domain, 32, 1,
		    "taller_intc", handle_level_irq, clr, 0, IRQ_GC_INIT_MASK_CACHE);
	if (ret) {
		pr_warn("Failed to allocate irq chips\n");
		goto err_domain_remove;
	}

	gc = irq_get_domain_generic_chip(taller_irq_domain, 0);
	gc->reg_base = base;
	gc->chip_types[0].regs.mask = TALLER_INTC_MASK;
	gc->chip_types[0].chip.irq_mask = irq_gc_mask_clr_bit;
	gc->chip_types[0].chip.irq_unmask = irq_gc_mask_set_bit;

	writel(0, base + TALLER_INTC_MASK);
	set_handle_irq(taller_handle_irq);

	return 0;

err_domain_remove:
	irq_domain_remove(taller_irq_domain);
err_unmap:
	iounmap(base);
	return ret;
}
IRQCHIP_DECLARE(taller_intc, "itcr,intc", taller_intc_init);
