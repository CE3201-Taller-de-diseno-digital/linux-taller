// SPDX-License-Identifier: GPL-2.0-only
/*
 * drivers/clocksource/timer-taller.c
 *
 * Copyright (C) 2022 Alejandro Soto <alejandro@34project.org>
 */

#include <linux/clk.h>
#include <linux/clocksource.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/sched_clock.h>

static u64 notrace taller_cyclecnt_read(void)
{
	u32 cycles;
	asm volatile("mrc p15, 0, %0, c15, c0, 0" : "=r" (cycles));
	return cycles;
}

static u64 taller_cyclecnt_cs_read(struct clocksource *cs)
{
	(void) cs;
	return taller_cyclecnt_read();
}

static struct clocksource cyclecnt_cs = {
	.name	= "taller-cyclecnt",
	.rating	= 350,
	.read	= taller_cyclecnt_cs_read,
	.mask	= CLOCKSOURCE_MASK(32),
};

static int __init taller_cyclecnt_init(struct device_node *np)
{
	int ret;
	struct clk *clk;
	unsigned long rate;

	clk = of_clk_get(np, 0);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	rate = clk_get_rate(clk);

	ret = clocksource_register_hz(&cyclecnt_cs, rate);
	if (ret)
		return ret;

	sched_clock_register(taller_cyclecnt_read, 32, rate);
	return 0;
}
TIMER_OF_DECLARE(taller, "itcr,cyclecnt", taller_cyclecnt_init);
